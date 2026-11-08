#!/usr/bin/env python3
"""Creates or updates the GitHub release for the current version.

Version: watch/package.json ("version"), the same for watchapp and phone app.
Release text: docs/releases/v<version>.md
Assets: FindMyAnything-<version>.apk (from the GitHub Actions build of HEAD,
signed with the release key) and FindMyAnything-<version>.pbw (watch/build/watch.pbw,
build it first with `cd watch && pebble build`).

Usage (token only in this process, never in the shell):
    ( set -a; . ~/.hermes/.env; set +a; python3 tools/release.py [--prerelease] )

Running it again for the same version replaces the text and the assets.
"""
import argparse
import hashlib
import io
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = "https://api.github.com"


def git(*args):
    return subprocess.check_output(["git", "-C", ROOT, *args], text=True).strip()


def repo_name():
    url = git("remote", "get-url", "origin")
    name = url.split("github.com")[-1].lstrip(":/")
    return name[:-4] if name.endswith(".git") else name


class GitHub:
    def __init__(self, token):
        self.token = token

    def call(self, method, url, data=None, ctype="application/json", raw=False):
        if not url.startswith("http"):
            url = API + url
        body = None
        if data is not None:
            body = data if isinstance(data, bytes) else json.dumps(data).encode()
        req = urllib.request.Request(url, data=body, method=method)
        req.add_header("Authorization", "Bearer " + self.token)
        req.add_header("Accept", "application/vnd.github+json")
        req.add_header("X-GitHub-Api-Version", "2022-11-28")
        if body is not None:
            req.add_header("Content-Type", ctype)
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                out = r.read()
        except urllib.error.HTTPError as e:
            if e.code == 404 and method == "GET":
                return None
            sys.exit(f"GitHub {method} {url.split('?')[0]}: {e.code} {e.read()[:300]!r}")
        if raw:
            return out
        return json.loads(out) if out else {}

    def download(self, url):
        """Artifact zips redirect to a storage host: follow without sending the token there."""
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, *a, **k):
                return None
        req = urllib.request.Request(url)
        req.add_header("Authorization", "Bearer " + self.token)
        req.add_header("Accept", "application/vnd.github+json")
        try:
            with urllib.request.build_opener(NoRedirect).open(req, timeout=120) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code not in (301, 302, 303, 307, 308):
                sys.exit(f"download {url}: {e.code}")
            location = e.headers["Location"]
        with urllib.request.urlopen(location, timeout=300) as r:
            return r.read()


def find_apk(gh, repo, sha, version, wait_s):
    """APK of the successful Android build of this commit; waits for a running build."""
    deadline = time.time() + wait_s
    while True:
        runs = gh.call("GET", f"/repos/{repo}/actions/workflows/android.yml/runs?head_sha={sha}&per_page=5")
        runs = (runs or {}).get("workflow_runs", [])
        done = [r for r in runs if r["status"] == "completed" and r["conclusion"] == "success"]
        if done:
            run = done[0]
            break
        if any(r["status"] != "completed" for r in runs) and time.time() < deadline:
            print("  waiting for the Android build ...")
            time.sleep(20)
            continue
        sys.exit(f"No successful Android build for {sha[:7]}. Push first and let GitHub Actions finish.")
    arts = gh.call("GET", f"/repos/{repo}/actions/runs/{run['id']}/artifacts")["artifacts"]
    if not arts:
        sys.exit(f"Build {run['id']} has no artifact")
    data = gh.download(arts[0]["archive_download_url"])
    z = zipfile.ZipFile(io.BytesIO(data))
    name = f"FindMyAnything-{version}.apk"
    if name not in z.namelist():
        sys.exit(f"{name} not in the build artifact ({z.namelist()}); version in package.json changed?")
    print(f"  APK from build #{run['run_number']} ({run['html_url']})")
    return name, z.read(name)


def check_pbw(version):
    path = os.path.join(ROOT, "watch", "build", "watch.pbw")
    if not os.path.exists(path):
        sys.exit("watch/build/watch.pbw missing: cd watch && pebble build")
    data = open(path, "rb").read()
    info = json.loads(zipfile.ZipFile(io.BytesIO(data)).read("appinfo.json"))
    if info.get("versionLabel") != version:
        sys.exit(f"PBW has version {info.get('versionLabel')}, package.json {version}: rebuild the watchapp")
    for marker in (b"github_pat_", b"sk-or-", b"sk-ant-"):
        if marker in data:
            sys.exit("PBW contains something that looks like a key - stop")
    return f"FindMyAnything-{version}.pbw", data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prerelease", action="store_true", help="mark as pre-release (not yet tested)")
    ap.add_argument("--wait", type=int, default=900, help="seconds to wait for a running Android build")
    args = ap.parse_args()

    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        sys.exit("GITHUB_TOKEN missing (run inside a subshell that sources the .env)")
    gh = GitHub(token)
    repo = repo_name()
    version = json.load(open(os.path.join(ROOT, "watch", "package.json")))["version"]
    tag = "v" + version
    notes_path = os.path.join(ROOT, "docs", "releases", tag + ".md")
    if not os.path.exists(notes_path):
        sys.exit(f"Release text missing: docs/releases/{tag}.md")
    if git("status", "--porcelain", "--untracked-files=no"):
        sys.exit("Uncommitted changes: commit and push first")
    sha = git("rev-parse", "HEAD")
    git("fetch", "-q", "origin")
    if git("rev-parse", "origin/main") != sha:
        sys.exit("HEAD is not origin/main: push first")

    print(f"{repo} {tag} at {sha[:7]}")
    pbw = check_pbw(version)
    apk = find_apk(gh, repo, sha, version, args.wait)

    body = open(notes_path, encoding="utf-8").read()
    meta = {"tag_name": tag, "target_commitish": sha, "name": f"Find My Anything {version}",
            "body": body, "prerelease": args.prerelease, "draft": False}
    rel = gh.call("GET", f"/repos/{repo}/releases/tags/{tag}")
    if rel:
        rel = gh.call("PATCH", f"/repos/{repo}/releases/{rel['id']}", {k: v for k, v in meta.items() if k != "tag_name"})
        print("  release updated")
    else:
        rel = gh.call("POST", f"/repos/{repo}/releases", meta)
        print("  release created")

    upload = rel["upload_url"].split("{")[0]
    existing = {a["name"]: a["id"] for a in rel.get("assets", [])}
    for name, data in (apk, pbw):
        if name in existing:
            gh.call("DELETE", f"/repos/{repo}/releases/assets/{existing[name]}")
        ctype = "application/vnd.android.package-archive" if name.endswith(".apk") else "application/octet-stream"
        a = gh.call("POST", f"{upload}?name={name}", data, ctype=ctype)
        print(f"  {name}: {a['size']} bytes, sha256 {hashlib.sha256(data).hexdigest()}")
        print(f"    {a['browser_download_url']}")
    print(rel["html_url"])


if __name__ == "__main__":
    main()
