#!/usr/bin/env python3
"""Single source for all app texts. Writes app/src/main/res/values*/strings.xml.

Also writes watch/src/pkjs/i18n.js for the settings page in the Pebble app:
English text -> translation, per language (format strings expanded with the
values the page uses, see PAGE_FORMATS).

Columns: key, en, de, fr, it, es, pt (Brazil), nl. English is the default (values/).
German: standard spelling (ß), informal "du"; other languages informal as well.
Plurals: "one|other". Brand and model names stay in the Kotlin code.

Run from android/:  python3 tools/strings.py
"""
import os

LANGS = ["en", "de", "fr", "it", "es", "pt", "nl"]

T = [
    # ---- Main screen ----
    ("status_scanning", "Searching", "Suche läuft", "Recherche en cours", "Ricerca in corso", "Buscando", "Buscando", "Zoeken actief"),
    ("status_stopped", "Search stopped", "Suche gestoppt", "Recherche arrêtée", "Ricerca interrotta", "Búsqueda detenida", "Busca parada", "Zoeken gestopt"),
    ("status_watch", "watch connected", "Uhr verbunden", "montre connectée", "orologio connesso", "reloj conectado", "relógio conectado", "horloge verbonden"),
    ("setup", "Setup", "Einrichtung", "Configuration", "Configurazione", "Configuración", "Configuração", "Instellen"),
    ("perm_basic", "Bluetooth and location permissions", "Bluetooth- und Standortrechte", "Autorisations Bluetooth et localisation", "Permessi Bluetooth e posizione", "Permisos de Bluetooth y ubicación", "Permissões de Bluetooth e localização", "Bluetooth- en locatierechten"),
    ("perm_background", "Location: “Allow all the time” (for searching with the watch)", "Standort: «Immer erlauben» (für die Suche mit der Uhr)", "Localisation : « Toujours autoriser » (pour chercher avec la montre)", "Posizione: «Consenti sempre» (per cercare con l'orologio)", "Ubicación: «Permitir siempre» (para buscar con el reloj)", "Localização: “Permitir o tempo todo” (para buscar com o relógio)", "Locatie: ‘Altijd toestaan’ (om met het horloge te zoeken)"),
    ("battery", "Battery optimisation off", "Akku-Optimierung aus", "Optimisation de la batterie désactivée", "Ottimizzazione batteria disattivata", "Optimización de batería desactivada", "Otimização de bateria desativada", "Batterijoptimalisatie uit"),
    ("pebble_app", "Pebble app installed", "Pebble-App installiert", "App Pebble installée", "App Pebble installata", "App Pebble instalada", "App Pebble instalado", "Pebble-app geïnstalleerd"),
    ("allow", "Allow", "Erlauben", "Autoriser", "Consenti", "Permitir", "Permitir", "Toestaan"),
    ("app_info", "Open app info", "App-Infos öffnen", "Ouvrir les infos de l'app", "Apri info app", "Abrir info de la app", "Abrir informações do app", "App-info openen"),
    ("open_on_watch", "Open on watch", "Auf der Uhr öffnen", "Ouvrir sur la montre", "Apri sull'orologio", "Abrir en el reloj", "Abrir no relógio", "Openen op horloge"),
    ("restart", "Restart", "Neu starten", "Redémarrer", "Riavvia", "Reiniciar", "Reiniciar", "Herstarten"),
    ("settings", "Settings", "Einstellungen", "Réglages", "Impostazioni", "Ajustes", "Configurações", "Instellingen"),
    ("back_to_list", "Back to list", "Zur Liste", "Retour à la liste", "Torna all'elenco", "Volver a la lista", "Voltar à lista", "Terug naar lijst"),
    ("paired", "paired", "gekoppelt", "associé", "associato", "vinculado", "pareado", "gekoppeld"),
    ("seen_ago", "%1$d s ago", "vor %1$d s", "il y a %1$d s", "%1$d s fa", "hace %1$d s", "há %1$d s", "%1$d s geleden"),
    ("no_signal", "no signal", "kein Signal", "aucun signal", "nessun segnale", "sin señal", "sem sinal", "geen signaal"),
    ("not_seen_for", "not seen for %1$d s", "seit %1$d s nicht gesehen", "non vu depuis %1$d s", "non visto da %1$d s", "sin ver desde hace %1$d s", "não visto há %1$d s", "%1$d s niet gezien"),
    ("warmer", "▲ warmer", "▲ wärmer", "▲ plus chaud", "▲ più caldo", "▲ más caliente", "▲ mais quente", "▲ warmer"),
    ("colder", "▼ colder", "▼ kälter", "▼ plus froid", "▼ più freddo", "▼ más frío", "▼ mais frio", "▼ kouder"),
    ("steady", "● steady", "● gleich", "● stable", "● stabile", "● estable", "● estável", "● gelijk"),
    ("best_dbm", "Best %1$d dBm", "Bestwert %1$d dBm", "Meilleur %1$d dBm", "Migliore %1$d dBm", "Mejor %1$d dBm", "Melhor %1$d dBm", "Beste %1$d dBm"),
    ("approx_dist", "approx. %1$s (rough)", "ca. %1$s (grob)", "env. %1$s (approximatif)", "circa %1$s (indicativo)", "aprox. %1$s (orientativo)", "aprox. %1$s (estimado)", "ca. %1$s (grof)"),
    ("readings", "%1$s readings/s", "%1$s Messungen/s", "%1$s mesures/s", "%1$s misure/s", "%1$s lecturas/s", "%1$s leituras/s", "%1$s metingen/s"),
    ("px_0", "right here", "direkt daneben", "tout près", "proprio qui", "justo aquí", "bem aqui", "vlak bij"),
    ("px_1", "very close", "sehr nah", "très proche", "molto vicino", "muy cerca", "muito perto", "heel dichtbij"),
    ("px_2", "close", "nah", "proche", "vicino", "cerca", "perto", "dichtbij"),
    ("px_3", "medium", "mittel", "moyen", "medio", "media", "médio", "middel"),
    ("px_4", "far", "weit", "loin", "lontano", "lejos", "longe", "ver"),
    ("px_5", "very far", "sehr weit", "très loin", "molto lontano", "muy lejos", "muito longe", "heel ver"),

    # ---- Settings ----
    ("s_watch_list", "Watch: list", "Uhr: Liste", "Montre : liste", "Orologio: elenco", "Reloj: lista", "Relógio: lista", "Horloge: lijst"),
    ("s_sort", "Sort by", "Sortieren nach", "Trier par", "Ordina per", "Ordenar por", "Ordenar por", "Sorteren op"),
    ("s_sort_signal", "Signal strength", "Signalstärke", "Force du signal", "Intensità del segnale", "Intensidad de la señal", "Intensidade do sinal", "Signaalsterkte"),
    ("s_sort_name", "Name", "Name", "Nom", "Nome", "Nombre", "Nome", "Naam"),
    ("s_sort_newest", "Newest first", "Neueste zuerst", "Plus récents d'abord", "Più recenti prima", "Más recientes primero", "Mais recentes primeiro", "Nieuwste eerst"),
    ("s_resort", "Re-sort by signal", "Nach Signal neu sortieren", "Retrier selon le signal", "Riordina per segnale", "Reordenar por señal", "Reordenar por sinal", "Opnieuw sorteren op signaal"),
    ("s_every", "every %1$d s", "alle %1$d s", "toutes les %1$d s", "ogni %1$d s", "cada %1$d s", "a cada %1$d s", "elke %1$d s"),
    ("s_resort_hint", "The list holds still while you scroll and for 5 s after. Tap the header on the watch to sort right away.",
     "Die Liste hält still, während du blätterst, und noch 5 s danach. Tippe auf der Uhr auf die Kopfzeile, um sofort zu sortieren.",
     "La liste reste immobile pendant que tu fais défiler et encore 5 s après. Touche l'en-tête sur la montre pour trier tout de suite.",
     "L'elenco resta fermo mentre scorri e per altri 5 s. Tocca l'intestazione sull'orologio per ordinare subito.",
     "La lista no se mueve mientras te desplazas y 5 s después. Toca la cabecera en el reloj para ordenar al momento.",
     "A lista fica parada enquanto você rola e por mais 5 s. Toque no cabeçalho do relógio para ordenar na hora.",
     "De lijst staat stil terwijl je scrolt en nog 5 s daarna. Tik op het horloge op de kopregel om meteen te sorteren."),
    ("s_start_filter", "Filter at start", "Filter beim Start", "Filtre au démarrage", "Filtro all'avvio", "Filtro al iniciar", "Filtro ao iniciar", "Filter bij start"),
    ("f_all", "All", "Alle", "Tous", "Tutti", "Todos", "Todos", "Alle"),
    ("f_tracker", "Trackers", "Tracker", "Traceurs", "Tracker", "Rastreadores", "Rastreadores", "Trackers"),
    ("f_audio", "Audio", "Audio", "Audio", "Audio", "Audio", "Áudio", "Audio"),
    ("f_phone_pc", "Phone & PC", "Handy & PC", "Téléphone & PC", "Telefono & PC", "Móvil y PC", "Celular e PC", "Telefoon & pc"),
    ("f_wearable", "Wearables", "Wearables", "Objets connectés", "Indossabili", "Wearables", "Vestíveis", "Wearables"),
    ("f_other", "Other", "Sonstige", "Autres", "Altri", "Otros", "Outros", "Overig"),
    ("s_big", "Big number in the search", "Große Zahl in der Suche", "Grand nombre pendant la recherche", "Numero grande nella ricerca", "Número grande en la búsqueda", "Número grande na busca", "Groot getal bij het zoeken"),
    ("s_big_pct", "Signal 0–100", "Signal 0–100", "Signal 0–100", "Segnale 0–100", "Señal 0–100", "Sinal 0–100", "Signaal 0–100"),
    ("s_sound_section", "Sound & vibration", "Ton & Vibration", "Son et vibration", "Suono e vibrazione", "Sonido y vibración", "Som e vibração", "Geluid en trillen"),
    ("s_feedback", "Signal at the start of a search", "Signal zu Beginn der Suche", "Signal au début de la recherche", "Segnale all'inizio della ricerca", "Señal al empezar la búsqueda", "Sinal no início da busca", "Signaal bij het begin van het zoeken"),
    ("fb_off", "Silent", "Stumm", "Muet", "Muto", "Silencio", "Mudo", "Stil"),
    ("fb_tone", "Sound", "Ton", "Son", "Suono", "Sonido", "Som", "Geluid"),
    ("fb_vibe", "Vibration", "Vibration", "Vibration", "Vibrazione", "Vibración", "Vibração", "Trillen"),
    ("fb_both", "Sound + vibration", "Ton + Vibration", "Son + vibration", "Suono + vibrazione", "Sonido + vibración", "Som + vibração", "Geluid + trillen"),
    ("s_feedback_hint", "On the watch, Select or a tap on the bottom line switches it during a search.",
     "Auf der Uhr wechselst du es in der Suche mit Select oder einem Tipp auf die unterste Zeile.",
     "Sur la montre, Select ou une touche sur la dernière ligne le change pendant la recherche.",
     "Sull'orologio lo cambi durante la ricerca con Select o toccando l'ultima riga.",
     "En el reloj se cambia durante la búsqueda con Select o tocando la última línea.",
     "No relógio, Select ou um toque na última linha muda isso durante a busca.",
     "Op het horloge wissel je dit tijdens het zoeken met Select of een tik op de onderste regel."),
    ("s_sound", "Sound", "Klang", "Son", "Suono", "Sonido", "Som", "Klank"),
    ("s_sound_beep", "Soft beep", "Weicher Piepton", "Bip doux", "Bip morbido", "Pitido suave", "Bipe suave", "Zachte piep"),
    ("s_sound_click", "Geiger counter click", "Geigerzähler-Klick", "Clic de compteur Geiger", "Clic del contatore Geiger", "Clic de contador Geiger", "Clique de contador Geiger", "Geigerteller-tik"),
    ("s_volume", "Volume", "Lautstärke", "Volume", "Volume", "Volumen", "Volume", "Volume"),
    ("s_vibe", "Vibration strength", "Vibrationsstärke", "Intensité de la vibration", "Intensità della vibrazione", "Intensidad de la vibración", "Intensidade da vibração", "Trilsterkte"),
    ("s_vibe_short", "Short", "Kurz", "Courte", "Breve", "Corta", "Curta", "Kort"),
    ("s_vibe_strong", "Strong", "Kräftig", "Forte", "Forte", "Fuerte", "Forte", "Sterk"),
    ("s_light_section", "Backlight", "Beleuchtung", "Rétroéclairage", "Retroilluminazione", "Retroiluminación", "Luz de fundo", "Verlichting"),
    ("s_light_auto", "Automatic", "Automatisch", "Automatique", "Automatica", "Automática", "Automática", "Automatisch"),
    ("s_light_search", "On during a search", "An während der Suche", "Allumé pendant la recherche", "Accesa durante la ricerca", "Encendida durante la búsqueda", "Acesa durante a busca", "Aan tijdens het zoeken"),
    ("s_light_always", "Always on in the app", "Immer an in der App", "Toujours allumé dans l'app", "Sempre accesa nell'app", "Siempre encendida en la app", "Sempre acesa no app", "Altijd aan in de app"),
    ("s_light_hint", "During a search, Down or a tap on the top of the watch screen turns the light on and off.",
     "In der Suche schaltest du das Licht mit Runter oder einem Tipp oben auf das Uhr-Display ein und aus.",
     "Pendant la recherche, Bas ou une touche en haut de l'écran de la montre allume ou éteint l'éclairage.",
     "Durante la ricerca, Giù o un tocco in alto sullo schermo dell'orologio accende e spegne la luce.",
     "Durante la búsqueda, Abajo o un toque arriba en la pantalla del reloj enciende y apaga la luz.",
     "Durante a busca, Para baixo ou um toque no topo da tela do relógio liga e desliga a luz.",
     "Tijdens het zoeken zet je het licht aan en uit met Omlaag of een tik bovenaan het horlogescherm."),
    ("s_phone_section", "Search on the phone", "Suche am Handy", "Recherche sur le téléphone", "Ricerca sul telefono", "Búsqueda en el móvil", "Busca no celular", "Zoeken op de telefoon"),
    ("s_classic", "Also search classic Bluetooth", "Auch klassisches Bluetooth suchen", "Chercher aussi le Bluetooth classique", "Cerca anche Bluetooth classico", "Buscar también Bluetooth clásico", "Buscar também Bluetooth clássico", "Ook klassieke Bluetooth zoeken"),
    ("s_classic_hint", "Finds older devices in pairing mode. Slows the BLE search down briefly every 45 s.",
     "Findet ältere Geräte im Kopplungsmodus. Bremst die BLE-Suche alle 45 s kurz.",
     "Trouve les anciens appareils en mode association. Ralentit brièvement la recherche BLE toutes les 45 s.",
     "Trova i dispositivi più vecchi in modalità abbinamento. Rallenta brevemente la ricerca BLE ogni 45 s.",
     "Encuentra dispositivos antiguos en modo de vinculación. Ralentiza un momento la búsqueda BLE cada 45 s.",
     "Encontra dispositivos antigos em modo de pareamento. Deixa a busca BLE mais lenta por um instante a cada 45 s.",
     "Vindt oudere apparaten in koppelmodus. Remt het BLE-zoeken elke 45 s even af."),
    ("s_min_rssi", "Hide weak signals", "Schwache Signale ausblenden", "Masquer les signaux faibles", "Nascondi i segnali deboli", "Ocultar señales débiles", "Ocultar sinais fracos", "Zwakke signalen verbergen"),
    ("s_off", "Off", "Aus", "Désactivé", "Disattivato", "Desactivado", "Desligado", "Uit"),
    ("s_below", "below %1$d dBm", "unter %1$d dBm", "sous %1$d dBm", "sotto %1$d dBm", "por debajo de %1$d dBm", "abaixo de %1$d dBm", "onder %1$d dBm"),
    ("s_hide_unnamed", "Hide devices without a name", "Geräte ohne Namen ausblenden", "Masquer les appareils sans nom", "Nascondi i dispositivi senza nome", "Ocultar dispositivos sin nombre", "Ocultar dispositivos sem nome", "Apparaten zonder naam verbergen"),
    ("s_hide_unnamed_hint", "Trackers are always shown.", "Tracker werden immer angezeigt.", "Les traceurs restent toujours affichés.", "I tracker restano sempre visibili.", "Los rastreadores se muestran siempre.", "Rastreadores são sempre mostrados.", "Trackers worden altijd getoond."),
    ("s_max_age", "Keep devices out of reach for", "Geräte außer Reichweite behalten für", "Garder les appareils hors de portée pendant", "Mantieni i dispositivi fuori portata per", "Mantener los dispositivos fuera de alcance durante", "Manter dispositivos fora de alcance por", "Apparaten buiten bereik bewaren voor"),
    ("s_language", "Language", "Sprache", "Langue", "Lingua", "Idioma", "Idioma", "Taal"),
    ("s_lang_auto", "Automatic (phone)", "Automatisch (Handy)", "Automatique (téléphone)", "Automatica (telefono)", "Automático (móvil)", "Automático (celular)", "Automatisch (telefoon)"),
    ("s_lang_hint", "Applies to the phone app and the watch.", "Gilt für die Handy-App und die Uhr.", "S'applique à l'app et à la montre.", "Vale per l'app e per l'orologio.", "Se aplica a la app y al reloj.", "Vale para o app e para o relógio.", "Geldt voor de app en het horloge."),
    ("s_watch_hint", "Changes reach the watch as soon as the app is open there.", "Änderungen gehen an die Uhr, sobald die App dort offen ist.", "Les modifications arrivent sur la montre dès que l'app y est ouverte.", "Le modifiche arrivano all'orologio appena l'app è aperta lì.", "Los cambios llegan al reloj en cuanto la app está abierta allí.", "As alterações chegam ao relógio assim que o app estiver aberto nele.", "Wijzigingen komen op het horloge zodra de app daar open is."),
    ("s_app_intro", "The same settings are in the Pebble app under Find My Anything › Settings. The last change counts.",
     "Die gleichen Einstellungen findest du auch in der Pebble-App unter Find My Anything › Einstellungen. Es gilt die letzte Änderung.",
     "Les mêmes réglages se trouvent dans l'app Pebble sous Find My Anything › Réglages. La dernière modification compte.",
     "Le stesse impostazioni sono nell'app Pebble in Find My Anything › Impostazioni. Vale l'ultima modifica.",
     "Los mismos ajustes están en la app Pebble, en Find My Anything › Ajustes. Cuenta el último cambio.",
     "As mesmas configurações estão no app Pebble em Find My Anything › Configurações. Vale a última alteração.",
     "Dezelfde instellingen staan in de Pebble-app onder Find My Anything › Instellingen. De laatste wijziging telt."),
    ("s_page_intro", "The same settings are in the Find My Anything phone app. The last change counts.",
     "Die gleichen Einstellungen findest du auch in der Find-My-Anything-App auf dem Handy. Es gilt die letzte Änderung.",
     "Les mêmes réglages se trouvent dans l'app Find My Anything du téléphone. La dernière modification compte.",
     "Le stesse impostazioni sono nell'app Find My Anything sul telefono. Vale l'ultima modifica.",
     "Los mismos ajustes están en la app Find My Anything del móvil. Cuenta el último cambio.",
     "As mesmas configurações estão no app Find My Anything do celular. Vale a última alteração.",
     "Dezelfde instellingen staan in de Find My Anything-app op je telefoon. De laatste wijziging telt."),
    ("s_dl_section", "Android app", "Android-App", "App Android", "App Android", "App de Android", "App Android", "Android-app"),
    ("s_dl_text", "Find My Anything needs its free Android app: it does the Bluetooth search. The app is not in the Play Store, it is on GitHub.",
     "Find My Anything braucht die kostenlose Android-App: Sie übernimmt die Bluetooth-Suche. Die App ist nicht im Play Store, sondern auf GitHub.",
     "Find My Anything a besoin de son app Android gratuite : c'est elle qui fait la recherche Bluetooth. L'app n'est pas sur le Play Store, elle est sur GitHub.",
     "Find My Anything ha bisogno della sua app Android gratuita: è lei a fare la ricerca Bluetooth. L'app non è nel Play Store, si trova su GitHub.",
     "Find My Anything necesita su app de Android gratuita: es la que hace la búsqueda Bluetooth. La app no está en Play Store, está en GitHub.",
     "O Find My Anything precisa do app Android gratuito: é ele que faz a busca Bluetooth. O app não está na Play Store, está no GitHub.",
     "Find My Anything heeft de gratis Android-app nodig: die doet het Bluetooth-zoeken. De app staat niet in de Play Store, maar op GitHub."),
    ("s_dl_button", "Download the Android app", "Android-App herunterladen", "Télécharger l'app Android", "Scarica l'app Android", "Descargar la app de Android", "Baixar o app Android", "Android-app downloaden"),
    ("save", "Save", "Speichern", "Enregistrer", "Salva", "Guardar", "Salvar", "Opslaan"),
    ("s_support", "Support", "Unterstützen", "Soutenir", "Supporta", "Apoyar", "Apoiar", "Steunen"),
    ("s_support_text", "Find My Anything is free and has no ads. If you enjoy it, a coffee is much appreciated.",
     "Find My Anything ist kostenlos und ohne Werbung. Wenn es dir Freude macht, freue ich mich über einen Kaffee.",
     "Find My Anything est gratuit et sans publicité. Si l'app te plaît, un café me fera très plaisir.",
     "Find My Anything è gratuito e senza pubblicità. Se ti piace, un caffè è molto apprezzato.",
     "Find My Anything es gratis y sin anuncios. Si te gusta, te agradezco mucho un café.",
     "Find My Anything é gratuito e sem anúncios. Se você gostar, um café é muito bem-vindo.",
     "Find My Anything is gratis en zonder advertenties. Vind je het fijn, dan waardeer ik een koffie enorm."),
    ("s_version", "Version %1$s", "Version %1$s", "Version %1$s", "Versione %1$s", "Versión %1$s", "Versão %1$s", "Versie %1$s"),

    # ---- Status and notification ----
    ("st_no_perm", "Permissions missing: open the phone app", "Rechte fehlen: Handy-App öffnen", "Autorisations manquantes : ouvre l'app", "Permessi mancanti: apri l'app", "Faltan permisos: abre la app", "Faltam permissões: abra o app", "Rechten ontbreken: open de app"),
    ("st_bt_off", "Bluetooth is off on the phone", "Bluetooth am Handy ist aus", "Le Bluetooth du téléphone est désactivé", "Il Bluetooth del telefono è spento", "El Bluetooth del móvil está apagado", "O Bluetooth do celular está desligado", "Bluetooth op de telefoon staat uit"),
    ("st_loc_off", "Location is off on the phone", "Standort am Handy ist aus", "La localisation du téléphone est désactivée", "La posizione del telefono è disattivata", "La ubicación del móvil está desactivada", "A localização do celular está desligada", "Locatie op de telefoon staat uit"),
    ("st_scan_blocked", "Scan blocked: restart Bluetooth", "Scan blockiert: Bluetooth neu starten", "Recherche bloquée : redémarre le Bluetooth", "Ricerca bloccata: riavvia il Bluetooth", "Búsqueda bloqueada: reinicia el Bluetooth", "Busca bloqueada: reinicie o Bluetooth", "Zoeken geblokkeerd: herstart Bluetooth"),
    ("st_scan_unsupported", "Scan not supported", "Scan nicht unterstützt", "Recherche non prise en charge", "Ricerca non supportata", "Búsqueda no compatible", "Busca não suportada", "Zoeken niet ondersteund"),
    ("st_scan_error", "Scan error %1$s", "Scan-Fehler %1$s", "Erreur de recherche %1$s", "Errore di ricerca %1$s", "Error de búsqueda %1$s", "Erro de busca %1$s", "Zoekfout %1$s"),
    ("lost_device", "Device lost", "Gerät verloren", "Appareil perdu", "Dispositivo perso", "Dispositivo perdido", "Dispositivo perdido", "Apparaat kwijt"),
    ("notif_title", "Find My Anything is searching", "Find My Anything sucht", "Find My Anything recherche", "Find My Anything sta cercando", "Find My Anything está buscando", "Find My Anything está buscando", "Find My Anything zoekt"),
    ("notif_text", "The watch shows Bluetooth devices nearby", "Die Uhr zeigt Bluetooth-Geräte in der Nähe", "La montre affiche les appareils Bluetooth à proximité", "L'orologio mostra i dispositivi Bluetooth vicini", "El reloj muestra los dispositivos Bluetooth cercanos", "O relógio mostra dispositivos Bluetooth por perto", "Het horloge toont Bluetooth-apparaten in de buurt"),

    # ---- Categories (numbers shared with the watch) ----
    ("cat_0", "Device", "Gerät", "Appareil", "Dispositivo", "Dispositivo", "Dispositivo", "Apparaat"),
    ("cat_1", "Tracker", "Tracker", "Traceur", "Tracker", "Rastreador", "Rastreador", "Tracker"),
    ("cat_2", "Phone/tablet", "Handy/Tablet", "Téléphone/tablette", "Telefono/tablet", "Móvil/tableta", "Celular/tablet", "Telefoon/tablet"),
    ("cat_3", "Computer", "Computer", "Ordinateur", "Computer", "Ordenador", "Computador", "Computer"),
    ("cat_4", "Audio", "Audio", "Audio", "Audio", "Audio", "Áudio", "Audio"),
    ("cat_5", "Wearable", "Wearable", "Objet connecté", "Indossabile", "Wearable", "Vestível", "Wearable"),
    ("cat_6", "TV/media", "TV/Medien", "TV/médias", "TV/media", "TV/multimedia", "TV/mídia", "Tv/media"),
    ("cat_7", "Beacon", "Beacon", "Balise", "Beacon", "Baliza", "Beacon", "Baken"),
    ("cat_8", "Smart home", "Smart Home", "Maison connectée", "Casa smart", "Hogar inteligente", "Casa inteligente", "Slim huis"),
    ("cat_9", "Input", "Eingabe", "Saisie", "Input", "Entrada", "Entrada", "Invoer"),
    ("cat_10", "Vehicle", "Fahrzeug", "Véhicule", "Veicolo", "Vehículo", "Veículo", "Voertuig"),
    ("cat_11", "Health", "Gesundheit", "Santé", "Salute", "Salud", "Saúde", "Gezondheid"),

    # ---- Device descriptions ----
    ("c_findmy_acc", "Find My accessory", "Find-My-Zubehör", "Accessoire Localiser", "Accessorio Dov'è", "Accesorio Encontrar", "Acessório Buscar", "Zoek mijn-accessoire"),
    ("c_findmy_device", "Find My device", "Find-My-Gerät", "Appareil Localiser", "Dispositivo Dov'è", "Dispositivo Encontrar", "Dispositivo Buscar", "Zoek mijn-apparaat"),
    ("c_apple_device", "Apple device", "Apple-Gerät", "Appareil Apple", "Dispositivo Apple", "Dispositivo Apple", "Dispositivo Apple", "Apple-apparaat"),
    ("c_separated", "separated · battery %1$s", "getrennt · Akku %1$s", "séparé · batterie %1$s", "separato · batteria %1$s", "separado · batería %1$s", "separado · bateria %1$s", "los · batterij %1$s"),
    ("c_bat_full", "full", "voll", "pleine", "carica", "llena", "cheia", "vol"),
    ("c_bat_medium", "medium", "mittel", "moyenne", "media", "media", "média", "half"),
    ("c_bat_low", "low", "schwach", "faible", "bassa", "baja", "fraca", "laag"),
    ("c_bat_critical", "critical", "kritisch", "critique", "critica", "crítica", "crítica", "kritiek"),
    ("c_with_owner", "with its owner", "beim Besitzer", "avec son propriétaire", "con il proprietario", "con su dueño", "com o dono", "bij de eigenaar"),
    ("c_apple_headphones", "Apple headphones", "Apple-Kopfhörer", "Écouteurs Apple", "Cuffie Apple", "Auriculares Apple", "Fones Apple", "Apple-koptelefoon"),
    ("c_airplay", "AirPlay device", "AirPlay-Gerät", "Appareil AirPlay", "Dispositivo AirPlay", "Dispositivo AirPlay", "Dispositivo AirPlay", "AirPlay-apparaat"),
    ("c_hotspot", "iPhone (hotspot)", "iPhone (Hotspot)", "iPhone (partage de connexion)", "iPhone (hotspot)", "iPhone (punto de acceso)", "iPhone (roteador)", "iPhone (hotspot)"),
    ("c_apple_nearby", "Apple nearby", "Apple in der Nähe", "Apple à proximité", "Apple nelle vicinanze", "Apple cerca", "Apple por perto", "Apple in de buurt"),
    ("c_android_device", "Android device", "Android-Gerät", "Appareil Android", "Dispositivo Android", "Dispositivo Android", "Dispositivo Android", "Android-apparaat"),
    ("c_windows_pc", "Windows PC", "Windows-PC", "PC Windows", "PC Windows", "PC con Windows", "PC Windows", "Windows-pc"),
    ("c_windows_laptop", "Windows laptop", "Windows-Laptop", "Portable Windows", "Portatile Windows", "Portátil Windows", "Notebook Windows", "Windows-laptop"),
    ("c_windows_tablet", "Windows tablet", "Windows-Tablet", "Tablette Windows", "Tablet Windows", "Tableta Windows", "Tablet Windows", "Windows-tablet"),
    ("c_linux_pc", "Linux PC", "Linux-PC", "PC Linux", "PC Linux", "PC con Linux", "PC Linux", "Linux-pc"),
    ("c_ms_device", "Microsoft device", "Microsoft-Gerät", "Appareil Microsoft", "Dispositivo Microsoft", "Dispositivo Microsoft", "Dispositivo Microsoft", "Microsoft-apparaat"),
    ("c_pairing_mode", "pairing mode", "Kopplungsmodus", "mode association", "modalità abbinamento", "modo de vinculación", "modo de pareamento", "koppelmodus"),
    ("c_find_hub", "Find Hub tracker", "Find-Hub-Tracker", "Traceur Find Hub", "Tracker Find Hub", "Rastreador Find Hub", "Rastreador Find Hub", "Find Hub-tracker"),
    ("c_find_hub_foreign", "Google Find Hub, unknown owner", "Google Find Hub, fremd", "Google Find Hub, inconnu", "Google Find Hub, estraneo", "Google Find Hub, ajeno", "Google Find Hub, desconhecido", "Google Find Hub, onbekend"),
    ("c_eddystone", "Eddystone beacon", "Eddystone-Beacon", "Balise Eddystone", "Beacon Eddystone", "Baliza Eddystone", "Beacon Eddystone", "Eddystone-baken"),
    ("c_smarttag_setup", "SmartTag, setup", "SmartTag, Einrichtung", "SmartTag, configuration", "SmartTag, configurazione", "SmartTag, configuración", "SmartTag, configuração", "SmartTag, instellen"),
    ("c_samsung_device", "Samsung device", "Samsung-Gerät", "Appareil Samsung", "Dispositivo Samsung", "Dispositivo Samsung", "Dispositivo Samsung", "Samsung-apparaat"),
    ("c_samsung_offline", "Samsung offline finding", "Samsung Offline-Finden", "Samsung recherche hors ligne", "Samsung ricerca offline", "Samsung búsqueda sin conexión", "Samsung busca offline", "Samsung offline zoeken"),
    ("c_tile", "Tile tracker", "Tile-Tracker", "Traceur Tile", "Tracker Tile", "Rastreador Tile", "Rastreador Tile", "Tile-tracker"),
    ("c_chipolo", "Chipolo tracker", "Chipolo-Tracker", "Traceur Chipolo", "Tracker Chipolo", "Rastreador Chipolo", "Rastreador Chipolo", "Chipolo-tracker"),
    ("c_contact_tracing", "Contact tracing", "Kontakt-Tracing", "Traçage des contacts", "Tracciamento contatti", "Rastreo de contactos", "Rastreamento de contatos", "Contactonderzoek"),
    ("c_heart_rate", "Heart rate", "Herzfrequenz", "Fréquence cardiaque", "Frequenza cardiaca", "Frecuencia cardíaca", "Frequência cardíaca", "Hartslag"),
    ("c_input_device", "Input device", "Eingabegerät", "Périphérique de saisie", "Periferica di input", "Dispositivo de entrada", "Dispositivo de entrada", "Invoerapparaat"),
    ("c_sensor", "Sensor", "Sensor", "Capteur", "Sensore", "Sensor", "Sensor", "Sensor"),
    ("c_sport_sensor", "Sport sensor", "Sport-Sensor", "Capteur sportif", "Sensore sportivo", "Sensor deportivo", "Sensor esportivo", "Sportsensor"),
    ("c_medical", "Medical", "Medizin", "Médical", "Medicale", "Médico", "Médico", "Medisch"),
]

PLURALS = [
    ("devices", "%d device|%d devices", "%d Gerät|%d Geräte", "%d appareil|%d appareils", "%d dispositivo|%d dispositivi",
     "%d dispositivo|%d dispositivos", "%d dispositivo|%d dispositivos", "%d apparaat|%d apparaten"),
]


def esc(s):
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    s = s.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
    if s.startswith("@") or s.startswith("?"):
        s = "\\" + s
    return s


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    res = os.path.join(here, "..", "app", "src", "main", "res")
    keys = [r[0] for r in T]
    assert len(keys) == len(set(keys)), "duplicate key"
    for row in T + PLURALS:
        assert len(row) == 1 + len(LANGS), row[0]
        n = row[1].count("%1$")
        for v in row[1:]:
            assert v.count("%1$") == n, (row[0], v)
    for li, lang in enumerate(LANGS):
        d = os.path.join(res, "values" if lang == "en" else f"values-{lang}")
        os.makedirs(d, exist_ok=True)
        out = ['<?xml version="1.0" encoding="utf-8"?>',
               "<!-- Generated by tools/strings.py - edit the table there. -->", "<resources>"]
        if lang == "en":
            out.append('    <string name="app_name" translatable="false">Find My Anything</string>')
        for row in T:
            out.append(f'    <string name="{row[0]}">{esc(row[1 + li])}</string>')
        for row in PLURALS:
            one, other = row[1 + li].split("|")
            out.append(f'    <plurals name="{row[0]}">')
            out.append(f'        <item quantity="one">{esc(one)}</item>')
            if lang in ("fr", "it", "es", "pt"):
                out.append(f'        <item quantity="many">{esc(other)}</item>')
            out.append(f'        <item quantity="other">{esc(other)}</item>')
            out.append("    </plurals>")
        out.append("</resources>")
        with open(os.path.join(d, "strings.xml"), "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")
    write_pkjs(os.path.join(here, "..", "..", "watch", "src", "pkjs", "i18n.js"))
    print(f"{len(T)} strings + {len(PLURALS)} plurals x {len(LANGS)} languages")


# Format strings on the settings page and the values they are shown with
PAGE_FORMATS = {"s_every": [5, 10, 30, 60], "s_below": [-95, -90, -85, -80]}


def write_pkjs(path):
    import json
    tables = {}
    for li, lang in enumerate(LANGS):
        if lang == "en":
            continue
        d = {}
        for row in T:
            en, tr = row[1], row[1 + li]
            if "%1$" in en:
                for v in PAGE_FORMATS.get(row[0], []):
                    d[en.replace("%1$d", str(v))] = tr.replace("%1$d", str(v))
            else:
                d[en] = tr
        tables[lang] = d
    with open(path, "w", encoding="utf-8") as f:
        f.write("// Generated by android/tools/strings.py - edit the table there.\n")
        f.write("// English text of the settings page -> translation, per language.\n")
        f.write("module.exports = " + json.dumps(tables, ensure_ascii=False, indent=1, sort_keys=True) + ";\n")


if __name__ == "__main__":
    main()
