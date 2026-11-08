// Watch texts in English, German, French, Italian, Spanish, Portuguese (Brazil) and Dutch.
// German: standard spelling, informal "du" as in the other SIDEffect's apps.
// Texts from the phone (device details, status) are translated by the Android app.
#include "i18n.h"

static int s_lang = LANG_EN;

static const char *const STR[S_COUNT][LANG_COUNT] = {
  [S_DEVICE_ONE] = {"1 device", "1 Gerät", "1 appareil", "1 dispositivo", "1 dispositivo", "1 dispositivo", "1 apparaat"},
  [S_DEVICES] = {"%d devices", "%d Geräte", "%d appareils", "%d dispositivi", "%d dispositivos", "%d dispositivos", "%d apparaten"},

  [S_F_ALL] = {"All", "Alle", "Tous", "Tutti", "Todos", "Todos", "Alle"},
  [S_F_TRACKER] = {"Trackers", "Tracker", "Traceurs", "Tracker", "Rastreadores", "Rastreadores", "Trackers"},
  [S_F_AUDIO] = {"Audio", "Audio", "Audio", "Audio", "Audio", "Áudio", "Audio"},
  [S_F_PHONE_PC] = {"Phone & PC", "Handy & PC", "Téléphone & PC", "Telefono & PC", "Móvil y PC", "Celular e PC", "Telefoon & pc"},
  [S_F_WEARABLE] = {"Wearables", "Wearables", "Objets connectés", "Indossabili", "Wearables", "Vestíveis", "Wearables"},
  [S_F_OTHER] = {"Other", "Sonstige", "Autres", "Altri", "Otros", "Outros", "Overig"},

  [S_FB_OFF] = {"Signal: silent", "Signal: stumm", "Signal : muet", "Segnale: muto", "Señal: silencio", "Sinal: mudo", "Signaal: stil"},
  [S_FB_TONE] = {"Signal: sound", "Signal: Ton", "Signal : son", "Segnale: suono", "Señal: sonido", "Sinal: som", "Signaal: geluid"},
  [S_FB_VIBE] = {"Signal: vibration", "Signal: Vibration", "Signal : vibration", "Segnale: vibrazione", "Señal: vibración", "Sinal: vibração", "Signaal: trillen"},
  [S_FB_BOTH] = {"Signal: sound + vibration", "Signal: Ton + Vibration", "Signal : son + vibration", "Segnale: suono + vibrazione", "Señal: sonido + vibración", "Sinal: som + vibração", "Signaal: geluid + trillen"},

  [S_PX_0] = {"right here", "direkt daneben", "tout près", "proprio qui", "justo aquí", "bem aqui", "vlak bij"},
  [S_PX_1] = {"very close", "sehr nah", "très proche", "molto vicino", "muy cerca", "muito perto", "heel dichtbij"},
  [S_PX_2] = {"close", "nah", "proche", "vicino", "cerca", "perto", "dichtbij"},
  [S_PX_3] = {"medium", "mittel", "moyen", "medio", "media", "médio", "middel"},
  [S_PX_4] = {"far", "weit", "loin", "lontano", "lejos", "longe", "ver"},
  [S_PX_5] = {"very far", "sehr weit", "très loin", "molto lontano", "muy lejos", "muito longe", "heel ver"},

  [S_CAT_0] = {"Device", "Gerät", "Appareil", "Dispositivo", "Dispositivo", "Dispositivo", "Apparaat"},
  [S_CAT_1] = {"Tracker", "Tracker", "Traceur", "Tracker", "Rastreador", "Rastreador", "Tracker"},
  [S_CAT_2] = {"Phone/tablet", "Handy/Tablet", "Téléphone/tablette", "Telefono/tablet", "Móvil/tableta", "Celular/tablet", "Telefoon/tablet"},
  [S_CAT_3] = {"Computer", "Computer", "Ordinateur", "Computer", "Ordenador", "Computador", "Computer"},
  [S_CAT_4] = {"Audio", "Audio", "Audio", "Audio", "Audio", "Áudio", "Audio"},
  [S_CAT_5] = {"Wearable", "Wearable", "Objet connecté", "Indossabile", "Wearable", "Vestível", "Wearable"},
  [S_CAT_6] = {"TV/media", "TV/Medien", "TV/médias", "TV/media", "TV/multimedia", "TV/mídia", "Tv/media"},
  [S_CAT_7] = {"Beacon", "Beacon", "Balise", "Beacon", "Baliza", "Beacon", "Baken"},
  [S_CAT_8] = {"Smart home", "Smart Home", "Maison connectée", "Casa smart", "Hogar inteligente", "Casa inteligente", "Slim huis"},
  [S_CAT_9] = {"Input", "Eingabe", "Saisie", "Input", "Entrada", "Entrada", "Invoer"},
  [S_CAT_10] = {"Vehicle", "Fahrzeug", "Véhicule", "Veicolo", "Vehículo", "Veículo", "Voertuig"},
  [S_CAT_11] = {"Health", "Gesundheit", "Santé", "Salute", "Salud", "Saúde", "Gezondheid"},

  [S_STARTING] = {"Starting search …", "Suche startet …", "Démarrage …", "Avvio della ricerca …", "Iniciando búsqueda …", "Iniciando a busca …", "Zoeken start …"},
  [S_WAKING] = {"Waking up the phone app", "Die Handy-App wird geweckt", "L'app du téléphone démarre", "Avvio dell'app sul telefono", "Activando la app del móvil", "Ativando o app do celular", "De telefoon-app wordt gestart"},
  [S_NO_PHONE] = {"No answer from the phone", "Keine Antwort vom Handy", "Pas de réponse du téléphone", "Nessuna risposta dal telefono", "Sin respuesta del móvil", "Sem resposta do celular", "Geen antwoord van de telefoon"},
  [S_NO_PHONE_HINT] = {
    "Is the Find My Anything app installed and set up on the phone?",
    "Ist die Find-My-Anything-App auf dem Handy installiert und eingerichtet?",
    "L'app Find My Anything est-elle installée et configurée sur le téléphone ?",
    "L'app Find My Anything è installata e configurata sul telefono?",
    "¿Está instalada y configurada la app Find My Anything en el móvil?",
    "O app Find My Anything está instalado e configurado no celular?",
    "Is de Find My Anything-app op de telefoon geïnstalleerd en ingesteld?"},
  [S_PHONE_SAYS] = {"Phone says:", "Handy meldet:", "Le téléphone indique :", "Il telefono segnala:", "El móvil indica:", "O celular informa:", "Telefoon meldt:"},
  [S_EMPTY_FILTER] = {"Nothing in this filter", "Nichts in diesem Filter", "Rien dans ce filtre", "Niente in questo filtro", "Nada en este filtro", "Nada neste filtro", "Niets in dit filter"},
  [S_EMPTY_FILTER_HINT] = {
    "Filter: %s. Long-press Select for the next filter.",
    "Filter: %s. Lang auf Select drücken für den nächsten Filter.",
    "Filtre : %s. Appui long sur Select pour le filtre suivant.",
    "Filtro: %s. Tieni premuto Select per il filtro successivo.",
    "Filtro: %s. Mantén pulsado Select para el siguiente filtro.",
    "Filtro: %s. Segure Select para o próximo filtro.",
    "Filter: %s. Houd Select ingedrukt voor het volgende filter."},
  [S_NOTHING] = {"Nothing found yet", "Noch nichts gefunden", "Rien trouvé pour l'instant", "Ancora niente trovato", "Aún no se ha encontrado nada", "Nada encontrado ainda", "Nog niets gevonden"},
  [S_KEEP_SEARCHING] = {"Still searching …", "Die Suche läuft weiter …", "La recherche continue …", "La ricerca continua …", "La búsqueda continúa …", "A busca continua …", "Er wordt verder gezocht …"},

  [S_PHONE_SILENT] = {"Phone not responding", "Handy antwortet nicht", "Le téléphone ne répond pas", "Il telefono non risponde", "El móvil no responde", "O celular não responde", "Telefoon reageert niet"},
  [S_WAIT_PHONE] = {"waiting for phone …", "warte auf Handy …", "attente du téléphone …", "in attesa del telefono …", "esperando al móvil …", "aguardando o celular …", "wacht op telefoon …"},
  [S_NOT_FOUND] = {"not found", "nicht gefunden", "introuvable", "non trovato", "no encontrado", "não encontrado", "niet gevonden"},
  [S_NO_SIGNAL_FOR] = {"no signal for %d s", "kein Signal seit %d s", "aucun signal depuis %d s", "nessun segnale da %d s", "sin señal desde hace %d s", "sem sinal há %d s", "geen signaal sinds %d s"},
  [S_BEST] = {"best %d", "Bestwert %d", "meilleur %d", "migliore %d", "mejor %d", "melhor %d", "beste %d"},
  [S_DIST_CM] = {"approx. %d cm", "ca. %d cm", "env. %d cm", "circa %d cm", "aprox. %d cm", "aprox. %d cm", "ca. %d cm"},
  [S_DIST_M] = {"approx. %d.%d m", "ca. %d,%d m", "env. %d,%d m", "circa %d,%d m", "aprox. %d,%d m", "aprox. %d,%d m", "ca. %d,%d m"},
  [S_RATE] = {"%d.%d readings/s", "%d,%d Messungen/s", "%d,%d mesures/s", "%d,%d misure/s", "%d,%d lecturas/s", "%d,%d leituras/s", "%d,%d metingen/s"},
  [S_SCREEN_OFF] = {"Phone screen off: slower", "Handy-Display aus: langsamer", "Écran du téléphone éteint : plus lent", "Schermo del telefono spento: più lento", "Pantalla del móvil apagada: más lento", "Tela do celular desligada: mais lento", "Telefoonscherm uit: trager"},

  [S_MUTED] = {"Sound is muted on the watch", "Ton ist auf der Uhr stumm", "Le son est coupé sur la montre", "Suono disattivato sull'orologio", "El sonido está silenciado en el reloj", "O som está mudo no relógio", "Geluid staat uit op het horloge"},
  [S_PEAK_RESET] = {"Best value reset", "Bestwert zurückgesetzt", "Meilleure valeur réinitialisée", "Valore migliore azzerato", "Mejor valor reiniciado", "Melhor valor zerado", "Beste waarde gewist"},
  [S_LIGHT_ON] = {"Light stays on", "Licht bleibt an", "Éclairage permanent", "Luce sempre accesa", "Luz siempre encendida", "Luz sempre acesa", "Licht blijft aan"},
  [S_LIGHT_AUTO] = {"Light automatic", "Licht automatisch", "Éclairage automatique", "Luce automatica", "Luz automática", "Luz automática", "Licht automatisch"},

  [S_AGO] = {"%d s ago · %s", "vor %d s · %s", "il y a %d s · %s", "%d s fa · %s", "hace %d s · %s", "há %d s · %s", "%d s geleden · %s"},
  [S_BONDED] = {"paired · %s", "gekoppelt · %s", "associé · %s", "associato · %s", "vinculado · %s", "pareado · %s", "gekoppeld · %s"},
};

void i18n_set_lang(int lang) { s_lang = (lang >= 0 && lang < LANG_COUNT) ? lang : LANG_EN; }
int i18n_lang(void) { return s_lang; }

int i18n_lang_from_locale(void) {
  const char *loc = i18n_get_system_locale();
  if (!loc) return LANG_EN;
  if (strncmp(loc, "de", 2) == 0) return LANG_DE;
  if (strncmp(loc, "fr", 2) == 0) return LANG_FR;
  if (strncmp(loc, "it", 2) == 0) return LANG_IT;
  if (strncmp(loc, "es", 2) == 0) return LANG_ES;
  if (strncmp(loc, "pt", 2) == 0) return LANG_PT;
  if (strncmp(loc, "nl", 2) == 0) return LANG_NL;
  return LANG_EN;
}

const char *tr(StrId id) {
  if (id >= S_COUNT) return "";
  const char *s = STR[id][s_lang];
  return s ? s : STR[id][LANG_EN];
}
