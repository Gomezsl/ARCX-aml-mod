// arcx_core.h - nucleo de Project ARCX: detecta skins (.dff con huesos) en modloader/ (ZyZ)
// y en ARCXLoader/ (cargador propio, una carpeta por skin) y se las pone al jugador.
// Va dentro del mismo .so que la interfaz (arcx_ui.h): no hay archivos de comunicacion, la interfaz llama a core::Command().
#pragma once
#include "skinscan.h"
#include "arcx_loader.h"
namespace core {

static std::string g_root;   // .../files/modloader (ZyZ ModLoader)
static std::string g_loaderRoot;   // .../files/ARCXLoader (cargador propio)
static bool g_zyzOn = true;        // apartado ZyZ ModLoader activado (Ajustes)
static bool g_loaderOn = true;     // apartado ARCXLoader activado (Ajustes)
static int g_zyzCount = 0, g_loaderCount = 0, g_loaderPngs = 0;

static void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsa::LogV("core", "ARCX", fmt, ap);
    va_end(ap);
}

// ---- funciones del juego (libGTASA.so 2.10 arm64) -----------------------------------------------
static void  (*RequestSpecialChar)(int, const char*, int);
static void  (*LoadAllRequestedModels)(bool);
static bool  (*HasSpecialCharLoaded)(int);
static void  (*SetMissionDoesntRequireSpecialChar)(int);
static void* (*FindPlayerPed)(int);
static void* (*FindPlayerVehicle)(int, bool);   // devuelve el vehiculo si el jugador va dentro
static void  (*SetModelIndex)(void*, unsigned int);   // CPed::SetModelIndex
static void  (*DeleteRwObject)(void*);                // CEntity::DeleteRwObject (opcional)
static void  (*RebuildPlayer)(void*, bool);           // CClothes::RebuildPlayer (opcional, para volver a CJ)
static void* (*GetModelInfoByName)(const char*, int*);   // CModelInfo::GetModelInfo(name, &index)
static int   (*AddImageToList)(const char*, bool);        // CStreaming::AddImageToList (opcional: registra el IMG propio; devuelve el slot)
static void (*CdDirAddItem)(void*, const void*);                     // CDirectory::AddItem (opcional: mete una entrada en el CDirectory del juego)
static int  (*CdDirFindItem)(void*, const char*, uint32_t*, uint32_t*); // CDirectory::FindItem (opcional: busca una entrada en el CDirectory; 1 = esta)
static int  (*TxdFindSlot)(const char*);                        // CTxdStore::FindTxdSlot (opcional: busca un .txd por nombre; -1 = no esta)
static int  (*TxdAddSlot)(const char*, const char*, bool);     // CTxdStore::AddTxdSlot (opcional: registra un .txd propio; devuelve el hueco)
static void (*InfoSetCdPosnAndSize)(void*, uint32_t, uint32_t); // CStreamingInfo::SetCdPosnAndSize (opcional: pone offset/tamano de un .txd)
static void (*RequestModel)(int, int);                   // CStreaming::RequestModel (opcional: pide un modelo/txd al streaming)
static void* (*RwStreamOpen)(int, int, const void*);       // RwStreamOpen (opcional: abre un flujo de RenderWare; 3.1.19 carga directa del .txd)
static void* (*RwTexDictionaryStreamRead)(void*);          // RwTexDictionaryStreamRead (opcional: lee un diccionario de texturas de un flujo)
static void  (*RwStreamClose)(void*, void*);               // RwStreamClose (opcional: cierra un flujo de RenderWare)
static void  (*TxdAddRef)(int);                            // CTxdStore::AddRef (opcional: sube el contador de referencias de un .txd)
static uint64_t (*CFileMgrOpenFile)(const char*, const char*); // CFileMgr::OpenFile (opcional: probar con que ruta abre el juego el IMG propio; 0 = no abre)
static void  (*CFileMgrCloseFile)(uint64_t);              // CFileMgr::CloseFile (opcional)
static void  (*CGame_Process_orig)();
static void  (*CGame_InitRestart_orig)();
static void  (*CGame_ShutRestart_orig)();

static int g_slot = 9;                    // slot de personaje especial (el 9 choca menos con los que cargan las misiones)
static const bool g_missions = true;      // la skin se mantiene durante las misiones (siempre activo)
static const bool g_vehicleSwap = true;   // cambiar de skin dentro del vehiculo (siempre activo)
static std::string g_title = "PROJECT ARCX";

// ---- estado --------------------------------------------------------------------------------------
static std::vector<skinscan::Skin> g_skins;
static std::atomic<int> g_pending{-2};   // -2 nada, -1 volver a CJ, >=0 indice en g_skins
static int g_frame = 0;
static int g_curSkin = -1;                 // -1 = CJ, >=0 indice de la skin puesta
static const volatile uint8_t* g_pauseUser = nullptr;   // CTimer::m_UserPause
static const volatile uint8_t* g_pauseCode = nullptr;   // CTimer::m_CodePause
static uint8_t* g_scriptSpace = nullptr;   // CTheScripts::ScriptSpace (para detectar misiones)
static int g_prevMission = 0;
static int g_missionLogs = 0;
static const int kOnMissionVar = 408;      // $ONMISSION en 2.10 (lo dice el log de ZyZ)

static int g_desired = -1;                 // skin que el jugador quiere llevar (-1 = CJ); sobrevive a cinematicas, misiones y guardados
static void (*FlushImmediately)(void*, bool);   // CPedIntelligence::FlushImmediately
static void (*AddInCarAnim)(const void*, void*, bool);   // CCarEnterExit::AddInCarAnim(const CVehicle&, CPed*, bool esConductor)
static bool (*VehIsDriver)(const void*, const void*);    // CVehicle::IsDriver(const CPed*) const
static bool (*VehIsPassenger)(const void*, const void*); // CVehicle::IsPassenger(const CPed*) const
static bool g_remember = false;            // opcion "recordar": mantener la ultima skin entre partidas e inicios del juego
static std::string g_lastName = "CJ";      // ultima skin elegida (se guarda en settings.txt)
static bool g_needReapply = false;
static int g_reapplyDelay = 0;
static int g_autoTries = 0;
static bool g_giveUp = false;
static int g_nextTryFrame = 0;
// Estado de una peticion en curso: se pide UNA sola vez y luego se espera a
// que cargue (sin volver a pedir). La comprobacion previa de 3.1.1-3.1.3
// (GetModelInfoByName ANTES de pedir) bloqueaba TODAS las skins: el nombre
// solo entra en la tabla del juego cuando RequestSpecialChar lo asigna al
// slot. Por eso en 3.1.3 ni las de ZyZ se ponian. Y en 3.1.2 el segundo
// RequestSpecialChar seguido para una skin sin archivo cerro el juego: por
// eso una skin que falla no se vuelve a pedir hasta pasados 10 s.
static int g_applyIdx = -2;         // skin que se esta pidiendo (-2 = ninguna en curso)
static int g_applyFrame = 0;        // frame en el que se pidio
static bool g_applyWaiting = false; // se pidio y se espera a que cargue
static std::string g_pendingName;   // nombre de la skin que se esta pidiendo (el indice cambia al reescanear; el nombre no)
static std::string g_coolName;      // skin que fallo hace poco (enfriamiento)
static int g_coolFrame = -100000;
// Lo ultimo que se le pidio al slot al juego: volver a pedir la MISMA skin
// para el mismo slot mientras su carga sigue en curso se cae dentro de
// CStreaming::RequestSpecialModel (crash real en 3.1.4: tocar 'suk', ir a CJ
// y volver a tocar 'suk' enseguida). Pedir otra skin distinta si es seguro
// (el slot solo cambia de objetivo); pedir la misma dos veces seguidas, no.
static std::string g_slotReqName;
static int g_slotReqFrame = -100000;
static int g_miOff = -1;                   // offset de CEntity::m_nModelIndex dentro del ped (se calibra solo)
static unsigned g_wornModelId = 0;
static const volatile uint8_t* g_cutPtr = nullptr;    // CCutsceneMgr::ms_running
static const volatile uint8_t* g_loadPtr = nullptr;   // CGenericGameStorage::ms_bLoading
static bool g_prevCut = false, g_prevLoading = false, g_sessionPending = false;
static int g_nullFrames = 0;

// ---- datos publicados para la interfaz (el hilo de render los lee con GetSnapshot) -------------------
static std::mutex g_pubMtx;
static std::vector<std::string> g_pubSkins;
static std::string g_pubCur = "CJ", g_pubStatus, g_pubTitle = "PROJECT ARCX";
static bool g_pubRemember = false;
static bool g_pubZyzOn = true, g_pubLoaderOn = true;
static int g_pubZyzCount = 0, g_pubLoaderCount = 0, g_pubLoaderPngs = 0;
static std::string g_status;        // "agachado", "peleando"...: motivo de la espera; vacio = libre
static std::string g_stateCur = "CJ";

struct Snapshot { std::vector<std::string> skins; std::string current, status, title; bool remember = false;
                  bool zyzOn = true, loaderOn = true; int zyzCount = 0, loaderCount = 0, loaderPngs = 0; };
static void GetSnapshot(Snapshot& o) {
    std::lock_guard<std::mutex> lk(g_pubMtx);
    o.skins = g_pubSkins;
    o.current = g_pubCur;
    o.status = g_pubStatus;
    o.title = g_pubTitle;
    o.remember = g_pubRemember;
    o.zyzOn = g_pubZyzOn;
    o.loaderOn = g_pubLoaderOn;
    o.zyzCount = g_pubZyzCount;
    o.loaderCount = g_pubLoaderCount;
    o.loaderPngs = g_pubLoaderPngs;
}
static void PublishState() {
    std::lock_guard<std::mutex> lk(g_pubMtx);
    g_pubCur = g_stateCur;
    g_pubStatus = g_status;
    g_pubZyzOn = g_zyzOn;
    g_pubLoaderOn = g_loaderOn;
    g_pubZyzCount = g_zyzCount;
    g_pubLoaderCount = g_loaderCount;
    g_pubLoaderPngs = g_loaderPngs;
}
static void WriteCurrent(const char* name) { g_stateCur = name; PublishState(); }
static void WriteStatus(const std::string& st) {
    if (st == g_status) return;
    g_status = st;
    PublishState();
}

static void SaveSettings() {        // ARCXConfigs.ini, seccion [General] (la interfaz guarda la suya en [Menu])
    { std::lock_guard<std::mutex> lk(g_pubMtx); g_pubRemember = g_remember; g_pubTitle = g_title; }
    const bool rem = g_remember;
    const std::string title = g_title, last = g_lastName;
    const bool zyz = g_zyzOn, ldr = g_loaderOn;
    vsa::CfgUpdate([&](vsa::Ini& ini) {
        auto& g = ini["General"];
        g["remember"] = rem ? "1" : "0";
        g["title"] = title;
        g["last"] = last;
        g["zyz"] = zyz ? "1" : "0";       // apartado ZyZ ModLoader activado
        g["loader"] = ldr ? "1" : "0";    // apartado ARCXLoader (propio) activado
    });
    PublishState();   // la interfaz ve el cambio enseguida: los interruptores ya no muestran estados viejos
}
static void LoadSettings() {
    vsa::Ini ini = vsa::CfgRead();
    if (ini.find("General") != ini.end()) {
        g_remember = vsa::GetInt(ini, "General", "remember", 0) == 1;
        std::string t = vsa::Get(ini, "General", "title");
        if (!t.empty()) g_title = t;
        std::string l = vsa::Get(ini, "General", "last");
        if (!l.empty()) g_lastName = l;
        g_zyzOn = vsa::GetInt(ini, "General", "zyz", 1) == 1;
        g_loaderOn = vsa::GetInt(ini, "General", "loader", 1) == 1;
    }
    // Migracion unica (3.1.4): los interruptores de 3.1.0-3.1.2 escribian valores
    // viejos en el INI (mostraban ON con el apartado apagado), asi que muchos
    // INI quedaron con zyz=0/loader=0 sin que el jugador lo pidiera. Una sola
    // vez se encienden los dos apartados; despues, lo que el jugador elija se respeta.
    if (vsa::GetInt(ini, "General", "srcfix", 0) == 0) {
        g_zyzOn = true; g_loaderOn = true;
        vsa::CfgUpdate([](vsa::Ini& i) { i["General"]["zyz"] = "1"; i["General"]["loader"] = "1"; i["General"]["srcfix"] = "1"; });
        Log("apartados ZyZ y ARCXLoader encendidos (migracion unica 3.1.4)");
    }
    std::lock_guard<std::mutex> lk(g_pubMtx);
    g_pubRemember = g_remember;
    g_pubTitle = g_title;
    g_pubZyzOn = g_zyzOn;        // si no se publican aqui, la interfaz arranca
    g_pubLoaderOn = g_loaderOn;  // mostrando los dos apartados en ON aunque el INI diga OFF
}
// marca la skin que se esta aplicando: si el juego se cierra en medio, al arrancar se avisa en el log
static void MarkApplying(const std::string& n) { vsa::CfgUpdate([&](vsa::Ini& ini) { ini["Debug"]["applying"] = n; }); }
static void ClearApplying() { vsa::CfgUpdate([&](vsa::Ini& ini) { ini["Debug"]["applying"] = ""; }); }

static void WriteList() {           // publica la lista y la deja tambien en ARCXConfigs.ini ([Skins]) como referencia
    {
        std::lock_guard<std::mutex> lk(g_pubMtx);
        g_pubSkins.clear();
        for (auto& s : g_skins) g_pubSkins.push_back(s.name);
    }
    vsa::CfgUpdate([&](vsa::Ini& ini) {
        auto& sec = ini["Skins"];
        sec.clear();
        sec["count"] = std::to_string(g_skins.size());
        for (size_t i = 0; i < g_skins.size(); i++) sec[std::to_string(i + 1)] = g_skins[i].name;
        auto& src = ini["Sources"];   // conteo por apartado (ZyZ / ARCXLoader)
        src.clear();
        src["zyz"] = std::to_string(g_zyzCount);
        src["loader"] = std::to_string(g_loaderCount);
        src["loader_pngs"] = std::to_string(g_loaderPngs);
    });
}

// ---- busqueda de skins en segundo plano (no frena el arranque del juego) -----------------------------
static std::atomic<bool> g_scanBusy{false}, g_scanReady{false};
static std::mutex g_scanMtx;
static std::vector<skinscan::Skin> g_scanOut;
static size_t g_scanRejected = 0;
static bool g_firstScan = true;

static void StartScan() {
    if (g_scanBusy.exchange(true)) return;
    const std::string root = g_root, loaderRoot = g_loaderRoot;
    const bool zyzOn = g_zyzOn, loaderOn = g_loaderOn;
    std::thread([root, loaderRoot, zyzOn, loaderOn]() {
        std::vector<skinscan::Skin> out;
        std::vector<std::string> rej;
        if (zyzOn) skinscan::Scan(root, out, &rej);          // apartado ZyZ (modloader/)
        std::vector<skinscan::Skin> lout;
        if (loaderOn) loader::ScanLoader(loaderRoot, lout, &rej);   // apartado propio (ARCXLoader/)
        for (auto& s : lout) {   // mismo nombre en los dos: se queda la propia
            bool dup = false;
            for (auto& o : out)
                if (skinscan::Lower(o.name) == skinscan::Lower(s.name)) {
                    LOGI("  '%s' existe en modloader/ y en ARCXLoader/: se queda la propia", s.name.c_str());
                    o = s; dup = true; break;
                }
            if (!dup) out.push_back(s);
        }
        std::sort(out.begin(), out.end(), [](const skinscan::Skin& a, const skinscan::Skin& b) { return skinscan::Lower(a.name) < skinscan::Lower(b.name); });
        loader::BuildImg(out, loaderRoot + "/arcxml/gta3.img");   // IMG propio (sin ZyZ): si no hay skins propias, no se construye
        { std::lock_guard<std::mutex> lk(g_scanMtx); g_scanOut = std::move(out); g_scanRejected = rej.size(); }
        g_scanReady = true;
        g_scanBusy = false;
    }).detach();
}

// hilo del juego: toma el resultado de la busqueda (los indices pueden cambiar: se recuerdan por nombre)
static void ConsumeScan() {
    if (!g_scanReady.exchange(false)) return;
    std::string desiredName = (g_desired >= 0 && g_desired < (int)g_skins.size()) ? g_skins[g_desired].name : "";
    std::string wornName = (g_curSkin >= 0 && g_curSkin < (int)g_skins.size()) ? g_skins[g_curSkin].name : "";
    std::string pendName = (g_pending.load() >= 0 && g_pending.load() < (int)g_skins.size()) ? g_skins[g_pending.load()].name : "";
    std::string applyName = g_pendingName;   // skin que se esta pidiendo (si hay una en curso)
    size_t rejected;
    {
        std::lock_guard<std::mutex> lk(g_scanMtx);
        g_skins = std::move(g_scanOut);
        rejected = g_scanRejected;
    }
    LOGI("%zu skin(s) en total: %s (ZyZ) + %s (ARCXLoader)", g_skins.size(), g_root.c_str(), g_loaderRoot.c_str());
    int zc = 0, lc = 0, lp = 0;
    for (auto& k : g_skins) {
        if (k.src == 1) { lc++; lp += k.pngCount + k.jpgCount; }
        else zc++;
    }
    g_zyzCount = zc; g_loaderCount = lc; g_loaderPngs = lp;
    loader::Rebuild(g_skins);   // la tabla de redireccion de archivos propios
    LOGI("  ZyZ: %d skin(s), ARCXLoader: %d skin(s) (%d png/jpg sueltos, solo se listan)", zc, lc, lp);
    for (auto& k : g_skins)
        LOGI("  + skin: %s  [%s] [chunk de skin/huesos: %s]%s%s", k.name.c_str(), k.src == 1 ? "ARCXLoader" : "modloader",
             k.ped ? "SI" : "NO", k.ped ? "" : "  <- NO es un ped valido: se bloquea (usa !nombre para forzar bajo tu riesgo)",
             (k.src == 1 && !k.hasTxd) ? "  <- sin .txd: se vera sin texturas" : "");
    LOGI("  (%zu .dff sin huesos ignorados: mapa, objetos, armas...)", rejected);
    auto find = [&](const std::string& n) {
        if (n.empty()) return -1;
        for (size_t i = 0; i < g_skins.size(); i++)
            if (skinscan::Lower(g_skins[i].name) == skinscan::Lower(n)) return (int)i;
        return -1;
    };
    if (!desiredName.empty()) g_desired = find(desiredName);
    if (!wornName.empty()) g_curSkin = find(wornName);
    // El indice de una peticion en curso queda viejo al cambiar la lista: se
    // vuelve a buscar por nombre. Sin esto, en 3.1.6 el tiempo de espera de
    // 'suk' se registro como 'Ghost' (el indice 0 paso a ser Ghost tras el
    // reescaneo al encender ZyZ) y Ghost quedo en enfriamiento sin haber
    // fallado. Si la skin ya no esta en la lista (su apartado se apago), se
    // cancela el pedido y el enfriamiento queda con el nombre correcto.
    if (!pendName.empty()) {
        int ni = find(pendName);
        if (ni >= 0) g_pending = ni; else g_pending = -2;
    }
    if (g_applyWaiting && g_applyIdx >= 0) {
        int ni = find(applyName);
        if (ni >= 0) { g_applyIdx = ni; }
        else {
            LOGI("la skin pendiente '%s' ya no esta en la lista: se cancela su pedido", applyName.c_str());
            g_applyWaiting = false; g_applyIdx = -2; g_pendingName.clear();
            g_coolName = applyName; g_coolFrame = g_frame;   // no se vuelve a pedir enseguida
            g_slotReqName.clear(); g_slotReqFrame = -100000; // el juego ya no la esta cargando para este slot
        }
    }
    if (g_firstScan) {               // "recordar": al arrancar se vuelve a querer la ultima skin
        g_firstScan = false;
        if (g_remember && g_lastName != "CJ") {
            int i = find(g_lastName);
            if (i >= 0) { g_desired = i; g_needReapply = true; g_reapplyDelay = 150; LOGI("ultima skin recordada: %s", g_lastName.c_str()); }
        }
    }
    WriteList();
    PublishState();   // conteos y apartados al momento: la interfaz ya no muestra estados viejos
}

// Debe llamarse en el hilo del juego (lo hacemos desde el hook de CGame::Process).
static std::atomic<bool> g_noDelete{false};
static void* g_pedVtbl = nullptr;   // vtable de CPedModelInfo (se toma de "male01")

static void (*ClearTaskDuckSecondary)(void*);   // CPedIntelligence::ClearTaskDuckSecondary
static int g_intelOff = -1;
static bool g_intelTried = false;
static const uintptr_t kPtrMask = 0x00FFFFFFFFFFFFFFull;

static bool Readable(const void* p, size_t n) {
    uintptr_t a = (uintptr_t)p & kPtrMask;
    if (a < 0x10000) return false;
    long pg = sysconf(_SC_PAGESIZE);
    uintptr_t st = a & ~((uintptr_t)pg - 1), en = (a + n - 1) & ~((uintptr_t)pg - 1);
    size_t pages = (size_t)((en - st) / (uintptr_t)pg) + 1;
    if (pages > 2) return false;
    unsigned char vec[2];
    return mincore((void*)st, pages * (size_t)pg, vec) == 0;
}

// CPedIntelligence empieza con m_pPed (puntero al ped): asi se localiza ped->m_pIntelligence sin conocer el offset
static void* FindIntelligence(void* ped) {
    if (g_intelOff < 0 && !g_intelTried) {
        g_intelTried = true;
        int found[4], n = 0;
        for (int o = 0x200; o < 0x700 && n < 4; o += 8) {
            void* p;
            memcpy(&p, (const char*)ped + o, sizeof p);
            if (((uintptr_t)p & 7) || !Readable(p, 8)) continue;
            uintptr_t first;
            memcpy(&first, p, sizeof first);
            if ((first & kPtrMask) == ((uintptr_t)ped & kPtrMask)) found[n++] = o;
        }
        LOGI("  calibracion CPedIntelligence: %d candidato(s)", n);
        for (int i = 0; i < n; i++) LOGI("    +0x%03x", found[i]);
        if (n == 1) { g_intelOff = found[0]; LOGI("  m_pIntelligence calibrado en ped+0x%03x", g_intelOff); }
        else LOGI("  m_pIntelligence SIN calibrar: no se limpia la tarea de agacharse");
    }
    if (g_intelOff < 0) return nullptr;
    void* p;
    memcpy(&p, (const char*)ped + g_intelOff, sizeof p);
    return p;
}

// agachado, el cambio de modelo dejaba la tarea de agacharse apuntando a animaciones destruidas (el jugador no podia caminar)
static void ClearDuck(void* ped) {
    if (!ClearTaskDuckSecondary) return;
    void* in = FindIntelligence(ped);
    if (in) ClearTaskDuckSecondary(in);
}

// ---- el cambio de modelo destruye las animaciones: agachado, peleando, en coche, saltando... deja al jugador atascado ----
static int g_busySince = 0;
static int g_layoutLogs = 0;

// CPedIntelligence = { CPed* m_pPed; CTaskManager { CTask* primarias[5]; CTask* secundarias[6]; CPed* m_pPed; } ... }
static bool InVehicle() { return FindPlayerVehicle && FindPlayerVehicle(-1, false); }

// ¿va realmente sentado? Al subir o bajar FindPlayerVehicle ya (o aun) devuelve el coche, pero el ped no es conductor ni pasajero.
static bool SeatedIn(void* veh, void* ped, bool* isDriver) {
    *isDriver = true;
    if (!veh || (!VehIsDriver && !VehIsPassenger)) return veh != nullptr;
    bool d = VehIsDriver && VehIsDriver(veh, ped);
    bool p = VehIsPassenger && VehIsPassenger(veh, ped);
    *isDriver = d || !p;
    return d || p;
}

// SetModelIndex deja al ped con una animacion de estar de pie (CAnimManager::AddAnimation). Dentro de un vehiculo hay que volver a
// ponerle la de sentado, como hace CTaskSimpleCarSetPedInAsDriver::ProcessPed (AddInCarAnim con true = conductor, false = pasajero).
static void LogTaskState(void* ped, const char* ctx) {   // para ajustar la deteccion de subir/bajar: tareas del jugador al aplicar
    static int n = 0;
    if (n >= 10) return;
    void* in = FindIntelligence(ped);
    if (!in || !Readable(in, 8 * 13)) return;
    uint64_t q[13];
    memcpy(q, in, sizeof q);
    n++;
    LOGI("  tareas %s: primarias=%d%d%d%d%d secundarias=%d%d%d%d%d%d", ctx, q[1] != 0, q[2] != 0, q[3] != 0, q[4] != 0, q[5] != 0,
         q[6] != 0, q[7] != 0, q[8] != 0, q[9] != 0, q[10] != 0, q[11] != 0);
}

static void ReseatInVehicle(void* ped, void* veh) {
    if (!veh || !AddInCarAnim) return;
    LogTaskState(ped, "en vehiculo al aplicar");
    bool drv;
    if (!SeatedIn(veh, ped, &drv)) { LOGI("  el jugador no va sentado (subiendo o bajando): no se restaura la animacion de sentado"); return; }
    AddInCarAnim(veh, ped, drv);
    LOGI("  en vehiculo: animacion de sentado restaurada (%s)", drv ? "conductor" : "pasajero");
}

// solo tareas (agachado, peleando, saltando...); el vehiculo se trata aparte
static bool PlayerBusy(void* ped, std::string* why) {
    void* in = FindIntelligence(ped);
    if (!in || !Readable(in, 8 * 13)) return false;
    uint64_t q[13];
    memcpy(q, in, sizeof q);
    bool layoutOk = ((q[12] & kPtrMask) == ((uintptr_t)ped & kPtrMask)) && q[5] != 0;   // m_pPed al final y tarea por defecto presente
    if (g_layoutLogs < 24) {
        g_layoutLogs++;
        LOGI("  tareas: primarias=%d%d%d%d%d secundarias=%d%d%d%d%d%d diseno=%s", q[1] != 0, q[2] != 0, q[3] != 0, q[4] != 0,
             q[5] != 0, q[6] != 0, q[7] != 0, q[8] != 0, q[9] != 0, q[10] != 0, q[11] != 0, layoutOk ? "ok" : "NO COINCIDE");
    }
    if (!layoutOk) return false;
    if (q[6]) { *why = "peleando"; return true; }
    if (q[7]) { *why = "agachado"; return true; }
    for (int i = 0; i < 4; i++)
        if (q[1 + i]) { *why = "ocupado"; return true; }
    return false;
}

// true = hay que esperar antes de cambiar el modelo. A pie el cambio es inmediato (se limpian las tareas);
// en vehiculo se espera a que salga salvo que la opcion "cambiar dentro del vehiculo" este activa.
static bool NeedWait(void* ped, std::string* why) {
    if (InVehicle()) {
        bool drv;
        if (!SeatedIn(FindPlayerVehicle(-1, false), ped, &drv)) { *why = "ocupado"; return true; }   // subiendo o bajando del coche
        // al bajar, IsDriver sigue siendo true hasta que el ped sale del todo: la tarea de bajar/subir (primarias) lo delata
        std::string tw;
        if (PlayerBusy(ped, &tw)) {
            static int vlog = 0;
            if (vlog++ < 12) LOGI("  en vehiculo: tareas activas (%s): se espera", tw.c_str());
            *why = (tw == "agachado") ? "ocupado" : tw;
            return true;
        }
        if (g_vehicleSwap) return false;
        *why = "en vehiculo";
        return true;
    }
    if (!FlushImmediately) return PlayerBusy(ped, why);   // sin forma de limpiar tareas: se espera
    return false;
}

// antes de destruir las animaciones: si el jugador esta agachado, peleando, etc. se limpian sus tareas como hace CLEAR_CHAR_TASKS_IMMEDIATELY
static void PrepareForModelChange(void* ped) {
    std::string why;
    if (!InVehicle() && FlushImmediately && PlayerBusy(ped, &why)) {
        void* in = FindIntelligence(ped);
        if (in) {
            LOGI("  jugador %s: se limpian sus tareas antes del cambio de modelo", why.c_str());
            FlushImmediately(in, true);
            return;
        }
    }
    ClearDuck(ped);
}

static int ReadPedModel(void* ped) {
    if (g_miOff < 0 || !ped) return -1;
    uint16_t v;
    memcpy(&v, (const char*)ped + g_miOff, sizeof v);
    return v;
}

static bool ApplySkin(int idx, bool force);
static bool ApplySkin(int idx, bool force = false) {
    void* ped = FindPlayerPed(-1);
    if (!ped) return false;                       // aun no hay jugador (menu / carga): se reintenta
    if (idx < 0) {                                // volver a CJ
        g_applyIdx = -2; g_applyWaiting = false; g_pendingName.clear();   // se abandona cualquier peticion en curso
        void* veh0 = InVehicle() ? FindPlayerVehicle(-1, false) : nullptr;
        PrepareForModelChange(ped);
        if (DeleteRwObject) DeleteRwObject(ped);
        SetModelIndex(ped, 0);
        ReseatInVehicle(ped, veh0);
        if (RebuildPlayer) RebuildPlayer(ped, false);
        if (g_miOff >= 0 && ReadPedModel(ped) != 0) { LOGI("  calibracion del modelo invalida (leo %d con CJ)", ReadPedModel(ped)); g_miOff = -1; }
        g_curSkin = -1;
        WriteCurrent("CJ");
        LOGI("skin: CJ");
        return true;
    }
    if (idx >= (int)g_skins.size()) return true;
    const std::string& name = g_skins[idx].name;
    if (!g_skins[idx].ped && !force) {
        LOGI("skin '%s' bloqueada: el .dff no tiene huesos (no es un ped). Crashearia el juego.", name.c_str());
        return true;
    }
    // ---- otra skin cargandose en el slot: no se pide una nueva hasta que termine ----
    // En 3.1.5 tocar varias skins seguidas (og->suk->wmy->xt) cambiaba el
    // objetivo del slot a cada toque y NINGUNA llegaba a cargar, ni las de
    // ZyZ (que en 3.1.4 cargaban bien una a una): el juego necesita que el
    // pedido termine antes de cambiar de objetivo. La peticion nueva queda
    // en cola (el watchdog vuelve a llamar) y arranca en cuanto la anterior
    // se resuelve. Si la anterior termino, no se aplica (el jugador ya
    // eligio otra): solo se libera el slot para la nueva.
    if (g_applyWaiting && g_applyIdx != idx) {
        if (HasSpecialCharLoaded(g_slot)) {
            g_applyWaiting = false; g_applyIdx = -2; g_pendingName.clear();
            g_slotReqName.clear(); g_slotReqFrame = -100000;
        } else if (g_frame - g_applyFrame > 300) {
            // La anterior se agoto sin cargar: se libera el slot para la
            // nueva (sin enfriamiento: si se vuelve a pedir la anterior, el
            // control de 3.1.5 por nombre ya frena el reintento 10 s).
            g_applyWaiting = false; g_applyIdx = -2; g_pendingName.clear();
        } else {
            return false;   // todavia cargando la anterior: se espera sin pedir
        }
    }
    // ---- fase 1: pedir (una sola vez por intento) ----
    // No hay comprobacion previa del nombre: GetModelInfoByName ANTES de pedir
    // siempre devuelve null para una skin nueva (el nombre solo entra en la
    // tabla cuando RequestSpecialChar lo asigna al slot), asi que en 3.1.1-3.1.3
    // NINGUNA skin se ponia, ni las de ZyZ. Se pide una vez y se espera.
    if (!g_applyWaiting || g_applyIdx != idx) {
        // Enfriamiento: una skin que fallo hace poco no se vuelve a pedir
        // enseguida (en 3.1.2 el segundo RequestSpecialChar seguido para una
        // skin sin archivo cerro el juego dentro de RequestSpecialModel).
        if (g_coolName == name && g_frame - g_coolFrame < 600) {
            LOGI("la skin '%s' fallo hace poco: espera unos segundos antes de volver a intentarlo", name.c_str());
            return true;   // hecho (rechazado): no se queda pendiente
        }
        if (g_slotReqName == name && g_frame - g_slotReqFrame < 600 && !force) {
            // Esta misma skin ya se le pidio a este slot hace poco: el juego
            // puede estar todavia cargandola y volver a pedirla se cae dentro
            // de RequestSpecialModel (crash real en 3.1.4: tocar 'suk', ir a
            // CJ y volver a tocar 'suk' enseguida; ir a CJ no cancela el
            // pedido que ya tiene el juego). No se vuelve a pedir: se espera
            // a que termine la carga que ya esta en curso.
            LOGI("la skin '%s' ya se pidio hace poco: se espera a que cargue sin volver a pedirla", name.c_str());
            g_applyIdx = idx;
            g_applyFrame = g_slotReqFrame;   // el tiempo de espera se cuenta desde el pedido original
            g_applyWaiting = true;
            g_pendingName = name;
        } else {
            if (g_skins[idx].src == 1) {
                // Skin PROPIA (ARCXLoader): se instala aqui el gancho de archivos
                // (diagnostico) y se registra el IMG propio en el juego, para que
                // el juego encuentre el archivo sin depender de ZyZ.
                loader::InstallHook();
                if (loader::ImgReady() && !loader::ImgRegistered() && AddImageToList)
                    loader::RegisterImg(AddImageToList, CFileMgrOpenFile, CFileMgrCloseFile, g_frame);
            }
            LOGI("aplicando '%s'...", name.c_str());
            RequestSpecialChar(g_slot, name.c_str(), 0);
            // 3.1.19: el .txd ya NO se pide al streaming aqui. Se carga DIRECTO
            // con RenderWare al registrar el IMG (LoadTxdDirect), asi que al
            // aplicar la skin su diccionario ya esta en el CTxdStore.
            LoadAllRequestedModels(false);
            g_slotReqName = name; g_slotReqFrame = g_frame;
            g_applyIdx = idx;
            g_applyFrame = g_frame;
            g_applyWaiting = true;
            g_pendingName = name;
        }
    }
    // ---- fase 2: esperar a que cargue (sin volver a pedir ni llenar el log) ----
    if (HasSpecialCharLoaded(g_slot)) {
        g_applyWaiting = false;
        g_applyIdx = -2;
        g_pendingName.clear();
        LOGI("  HasSpecialCharLoaded = 1");
        if (g_skins[idx].src == 1) loader::LogTxdStatus(name.c_str());   // diagnostico 3.1.13: ¿el .txd termino de cargar?
    } else if (g_frame - g_applyFrame > 300) {   // ~5 s sin cargar: se cancela sin crash
        g_applyWaiting = false;
        g_applyIdx = -2;
        g_pendingName.clear();
        g_coolName = name; g_coolFrame = g_frame;
        if (g_skins[idx].src == 1)
            LOGI("skin '%s' no cargo: el juego no encontro su .dff/.txd en el IMG propio (revisa en el log que diga 'directorio del IMG cargado en el juego (slot N)' y 'CDirectory del juego localizado'; si falta, el CDirectory no se localizo o el IMG no se registro)", name.c_str());
        else
            LOGI("skin '%s' no cargo (mira ZyZModLoader.log: nombre, .dff valido, textura)", name.c_str());
        return true;   // hecho (cancelado): no se queda pendiente
    } else {
        return false;   // todavia esperando: el watchdog vuelve a llamar sin volver a pedir
    }
    unsigned modelId = 290u + g_slot;   // valor por defecto; se corrige con el indice real que devuelve el juego
    {   // el juego dice en que id registro el modelo (RequestSpecialChar slot 1 -> id 291, no 290)
        int mIdx = -1;
        void* mi = GetModelInfoByName ? GetModelInfoByName(name.c_str(), &mIdx) : nullptr;
        if (mIdx >= 0) modelId = (unsigned)mIdx;
        LOGI("  GetModelInfo('%s') -> %p, id real del modelo = %d", name.c_str(), mi, mIdx);
        if (mIdx >= 0 && (mIdx < 290 || mIdx > 310) && !force) {
            LOGI("  id fuera del rango de personajes especiales (290-310): no se aplica");
            return true;
        }
        if (GetModelInfoByName && !mi && !force) {
            LOGI("  el juego no registro el nombre '%s' como modelo: no se aplica", name.c_str());
            return true;
        }
        if (mi && GetModelInfoByName) {   // el modelo debe ser un CPedModelInfo (el slot 10 -> id 300 no lo es y crashea)
            if (!g_pedVtbl) {
                int vi = -1;
                void* ref = GetModelInfoByName("male01", &vi);
                if (ref) g_pedVtbl = *(void**)ref;
                LOGI("  referencia de ped (male01, id %d): vtable %p", vi, g_pedVtbl);
            }
            if (g_pedVtbl && *(void**)mi != g_pedVtbl && !force) {
                LOGI("  el modelo %u (slot %d) NO es un ped (vtable %p): no se aplica", modelId, g_slot, *(void**)mi);
                if (g_slot != 1) { g_slot = 1; LOGI("  se usa el slot 1"); g_slotReqName.clear(); g_slotReqFrame = -100000; g_applyWaiting = false; g_applyIdx = -2; g_pendingName.clear(); return ApplySkin(idx, force); }
                return true;
            }
        }
        if (mi) {
            const uint64_t* q = (const uint64_t*)mi;
            for (int i = 0; i < 12; i += 4)
                LOGI("  modelinfo+%02x: %016llx %016llx %016llx %016llx", i * 8,
                     (unsigned long long)q[i], (unsigned long long)q[i + 1],
                     (unsigned long long)q[i + 2], (unsigned long long)q[i + 3]);
            void* rw = (void*)q[8];   // CBaseModelInfo::m_pRwObject: +0x40 en 64 bits (va justo despues del 2000.0 de drawDistance)
            LOGI("  m_pRwObject (+0x40) = %p", rw);
            if (!rw && !force) {
                LOGI("  el modelo esta 'cargado' pero no tiene clump: no se aplica (usa !%s para forzar)", name.c_str());
                return true;
            }
        }
    }
    PrepareForModelChange(ped);
    void* veh = InVehicle() ? FindPlayerVehicle(-1, false) : nullptr;
    if (DeleteRwObject && !g_noDelete.load()) { LOGI("  DeleteRwObject(ped=%p)", ped); DeleteRwObject(ped); }
    LOGI("  SetModelIndex(ped=%p, modelo=%u)", ped, modelId);
    SetModelIndex(ped, modelId);
    LOGI("  SetModelIndex termino sin crash");
    ReseatInVehicle(ped, veh);
    if (g_miOff >= 0 && ReadPedModel(ped) != (int)modelId) {
        LOGI("  calibracion del modelo invalida (leo %d, esperaba %u)", ReadPedModel(ped), modelId);
        g_miOff = -1;
    }
    if (g_miOff < 0) {   // busca donde guarda el ped su indice de modelo (uint16 == modelId)
        int found[8], n = 0, pick = -1;
        for (int o = 0x20; o < 0x100 && n < 8; o += 2) {
            uint16_t v;
            memcpy(&v, (const char*)ped + o, sizeof v);
            if (v == modelId) found[n++] = o;
        }
        LOGI("  calibracion m_nModelIndex: %d candidato(s)", n);
        for (int i = 0; i < n; i++) { LOGI("    +0x%02x", found[i]); if (found[i] == 0x2E) pick = 0x2E; }
        if (pick < 0 && n == 1) pick = found[0];
        g_miOff = pick;
        if (pick >= 0) LOGI("  m_nModelIndex calibrado en ped+0x%02x", pick);
        else LOGI("  m_nModelIndex SIN calibrar: no se detectan resets por modelo (solo por cinematica/mision/carga)");
    }
    g_wornModelId = modelId;
    SetMissionDoesntRequireSpecialChar(g_slot);
    g_curSkin = idx;
    WriteCurrent(name.c_str());
    LOGI("skin: %s", name.c_str());
    return true;
}

static std::atomic<bool> g_force{false};
static std::mutex g_cmdMtx;
static std::string g_cmd;           // pedido pendiente: lo deja la interfaz (hilo de render) y lo atiende el hilo del juego

static void Command(const std::string& c) { std::lock_guard<std::mutex> lk(g_cmdMtx); g_cmd = c; }

// El jugador elige una skin (-1 = CJ): se aplica y pasa a ser la deseada.
static void Choose(int idx) {
    g_pending = idx;
    g_desired = idx;
    g_lastName = idx >= 0 ? g_skins[idx].name : "CJ";
    g_needReapply = false; g_giveUp = false; g_autoTries = 0;
    SaveSettings();
}

static void ProcessCommand() {
    std::string s;
    { std::lock_guard<std::mutex> lk(g_cmdMtx); s.swap(g_cmd); }
    s = vsa::Trim(s);
    if (s.empty()) return;
    g_force = false;
    g_noDelete = false;
    while (!s.empty() && (s[0] == '!' || s[0] == '~')) {   // ! = forzar, ~ = no llamar DeleteRwObject
        if (s[0] == '!') g_force = true; else g_noDelete = true;
        s.erase(s.begin());
    }
    std::string l = skinscan::Lower(s);
    if (l.empty()) return;
    if (l == "remember_on" || l == "remember_off") {
        g_remember = (l == "remember_on");
        SaveSettings();
        LOGI("recordar ultima skin: %s", g_remember ? "SI" : "NO");
        return;
    }
    if (l == "zyz_on" || l == "zyz_off") {
        g_zyzOn = (l == "zyz_on");
        SaveSettings();
        LOGI("apartado ZyZ ModLoader: %s", g_zyzOn ? "SI" : "NO");
        StartScan();
        return;
    }
    if (l == "loader_on" || l == "loader_off") {
        g_loaderOn = (l == "loader_on");
        SaveSettings();
        LOGI("apartado ARCXLoader (propio): %s", g_loaderOn ? "SI" : "NO");
        if (!g_loaderOn) loader::Rebuild(std::vector<skinscan::Skin>{});   // tabla vacia: nada se redirige
        StartScan();
        return;
    }
    if (l == "cj" || l == "default" || l == "0") { Choose(-1); return; }
    if (l == "rescan" || l == "rescan_zyz" || l == "rescan_loader") { StartScan(); return; }
    if (l.find_first_not_of("0123456789") == std::string::npos) {
        int n = atoi(l.c_str()) - 1;
        if (n >= 0 && n < (int)g_skins.size()) { Choose(n); return; }
        LOGI("pedido: numero %s fuera de la lista", l.c_str());
        return;
    }
    for (size_t i = 0; i < g_skins.size(); i++)
        if (skinscan::Lower(g_skins[i].name) == l) { Choose((int)i); return; }
    LOGI("pedido: '%s' no es una skin conocida", s.c_str());
}

static bool ReadFlag(const volatile uint8_t* p) { return p && *p != 0; }

// El juego recreo al jugador (partida nueva o cargada).
static void OnNewSession() {
    g_curSkin = -1;
    g_slotReqName.clear(); g_slotReqFrame = -100000;   // partida nueva/cargada: el juego ya no tiene ningun pedido en el slot
    loader::ResetRegistration();   // el juego reconstruye su CDirectory al cargar el nivel: el IMG propio hay que volver a registrarlo
    g_giveUp = false; g_autoTries = 0;
    if (!g_remember) {
        g_desired = -1; g_lastName = "CJ"; g_needReapply = false;
        SaveSettings();
        WriteCurrent("CJ");
        LOGI("partida nueva/cargada: recordar=NO, se queda CJ");
    } else {
        g_needReapply = g_desired >= 0;
        g_reapplyDelay = 150;
        LOGI("partida nueva/cargada: recordar=SI, skin pendiente: %s", g_lastName.c_str());
    }
}

static void Watchdog() {
    void* ped = FindPlayerPed(-1);
    bool cut = ReadFlag(g_cutPtr), loading = ReadFlag(g_loadPtr);

    if (loading && !g_prevLoading) g_sessionPending = true;
    g_prevLoading = loading;
    if (!ped) { if (g_nullFrames < 1000) g_nullFrames++; g_prevCut = cut; return; }
    if (g_nullFrames >= 5) g_sessionPending = true;
    g_nullFrames = 0;
    if (g_sessionPending && !loading) { g_sessionPending = false; OnNewSession(); }

    if (g_prevCut && !cut) {   // fin de cinematica: el juego dejo a CJ, se vuelve a poner la skin
        LOGI("cinematica terminada");
        if (g_desired >= 0) { g_needReapply = true; g_reapplyDelay = 45; g_giveUp = false; g_autoTries = 0; }
    }
    g_prevCut = cut;

    int om = g_prevMission;
    if (g_scriptSpace) memcpy(&om, g_scriptSpace + kOnMissionVar * 4, sizeof om);
    if (g_scriptSpace && om != g_prevMission) {
        if (g_missionLogs++ < 20) LOGI("ONMISSION %d -> %d", g_prevMission, om);
        if (om != 0 && g_prevMission == 0 && !g_missions && g_curSkin >= 0 && g_pending.load() == -2) {
            LOGI("mision iniciada: se vuelve a CJ (la skin deseada se conserva)");
            g_pending = -1;     // g_desired no cambia
        }
        if (om == 0 && g_prevMission != 0 && g_desired >= 0 && g_curSkin != g_desired) {
            LOGI("mision terminada");
            g_needReapply = true; g_reapplyDelay = 60; g_giveUp = false; g_autoTries = 0;
        }
        g_prevMission = om;
    }

    // IMG propio: se registra en el juego en cuanto esta construido y el jugador esta en partida
    if (loader::ImgReady() && !loader::ImgRegistered() && !cut && !loading) {
        if (!AddImageToList) {
            static bool said = false;
            if (!said) { said = true; LOGI("AddImageToList no esta en libGTASA.so: las skins propias no se registraran (las de ZyZ si funcionan)"); }
        } else loader::RegisterImg(AddImageToList, CFileMgrOpenFile, CFileMgrCloseFile, g_frame);
    }

    // en pausa (menu, mapa, guardar partida) no se toca el modelo: se espera a que se reanude
    if ((g_pauseUser && *g_pauseUser) || (g_pauseCode && *g_pauseCode)) return;

    // peticiones explicitas: esperan a que acabe la cinematica o la carga, y a que el jugador este libre
    int p = g_pending.load();
    if (p != -2 && !cut && !loading) {
        std::string why;
        if (NeedWait(ped, &why)) {
            if (g_busySince == 0) { g_busySince = g_frame; LOGI("esperando: el jugador esta %s (cambiar el modelo ahora lo dejaria atascado)", why.c_str()); }
            WriteStatus(why);
            if (g_frame - g_busySince > 1200) {
                LOGI("se cancela el cambio: el jugador siguio ocupado (%s)", why.c_str());
                g_pending = -2; g_busySince = 0; WriteStatus("");
            }
        } else {
            g_busySince = 0;
            WriteStatus("");
            // Se marca una sola vez por intento (antes se reescribia el INI en cada frame mientras se esperaba).
            if (!g_applyWaiting) MarkApplying(p >= 0 && p < (int)g_skins.size() ? g_skins[p].name : std::string("CJ"));
            bool ok = ApplySkin(p, g_force.load());
            if (ok) { ClearApplying(); g_pending = -2; }   // listo (puesto, rechazado o cancelado); si sigue esperando, ApplySkin devuelve false y se vuelve a llamar
        }
    }

    bool allowed = !cut && !loading && (om == 0 || g_missions) && g_pending.load() == -2;
    if (allowed && g_desired >= 0 && g_curSkin == g_desired && g_miOff >= 0) {
        int cur = ReadPedModel(ped);   // el guardado o la carga pudieron devolver al jugador a CJ
        if (cur != (int)g_wornModelId) {
            static int mlog = 0;
            if (mlog++ < 20) LOGI("el modelo del jugador cambio a %d (esperado %u): se vuelve a poner la skin", cur, g_wornModelId);
            g_curSkin = -1;
            g_needReapply = true; g_reapplyDelay = 20;
        }
    }
    if (g_needReapply && allowed && g_desired >= 0 && g_desired < (int)g_skins.size() && !g_giveUp) {
        if (g_reapplyDelay > 0) {
            g_reapplyDelay--;
        } else if (g_frame >= g_nextTryFrame) {
            std::string why;
            if (NeedWait(ped, &why)) {
                static int lastBusyLog = -1000;
                if (g_frame - lastBusyLog > 600) { lastBusyLog = g_frame; LOGI("reaplicado en espera: el jugador esta %s", why.c_str()); }
                WriteStatus(why);
                return;
            }
            WriteStatus("");
            if (!g_applyWaiting) MarkApplying(g_skins[g_desired].name);
            bool done = ApplySkin(g_desired);
            if (done) {
                ClearApplying();
                if (g_curSkin == g_desired) g_needReapply = false;   // puesta
                else if (++g_autoTries > 3) {
                    g_giveUp = true; g_needReapply = false;
                    LOGI("no se pudo mantener la skin tras 3 intentos: se detiene el reintento automatico");
                } else g_nextTryFrame = g_frame + 90;   // se vuelve a intentar despues
            }
            // si sigue esperando (done == false), se vuelve a llamar en el siguiente frame sin contar intento
        }
    }
}

static void CGame_InitRestart_hook() {
    g_sessionPending = true;
    LOGI("CGame::InitialiseWhenRestarting (partida nueva o cargada)");
    CGame_InitRestart_orig();
}
static void CGame_ShutRestart_hook() {
    g_sessionPending = true;
    LOGI("CGame::ShutDownForRestart (partida nueva o cargada)");
    CGame_ShutRestart_orig();
}

static void CGame_Process_hook() {
    CGame_Process_orig();
    if (g_frame == 0) LOGI("hook de CGame::Process activo");
    ++g_frame;
    if (g_frame % 3 == 0) ProcessCommand();
    ConsumeScan();
    Watchdog();
}

// Resuelve los simbolos de libGTASA.so, pone los hooks del juego y lanza la busqueda de skins.
inline void Init(const std::string& modloaderPath, const std::string& loaderPath) {
    g_root = modloaderPath;
    g_loaderRoot = loaderPath;
    LoadSettings();
    WriteCurrent("CJ");   // proceso nuevo = CJ; con "recordar" se actualiza al reaplicar
    LOGI("recordar ultima skin: %s, ultima: %s", g_remember ? "SI" : "NO", g_lastName.c_str());
    LOGI("modloader: %s (ZyZ: %s)", g_root.c_str(), g_zyzOn ? "SI" : "NO");
    LOGI("ARCXLoader: %s (propio: %s)", g_loaderRoot.c_str(), g_loaderOn ? "SI" : "NO");

    void* h = aml->GetLibHandle("libGTASA.so");
    #define SYM(var, name) do { *(void**)&var = (void*)aml->GetSym(h, name); LOGI("  %s %s", var ? "ok     " : "FALTA  ", name); } while (0)
    SYM(RequestSpecialChar,             "_ZN10CStreaming18RequestSpecialCharEiPKci");
    SYM(LoadAllRequestedModels,         "_ZN10CStreaming22LoadAllRequestedModelsEb");
    SYM(HasSpecialCharLoaded,           "_ZN10CStreaming20HasSpecialCharLoadedEi");
    SYM(SetMissionDoesntRequireSpecialChar, "_ZN10CStreaming34SetMissionDoesntRequireSpecialCharEi");
    SYM(FindPlayerPed,                  "_Z13FindPlayerPedi");
    SYM(FindPlayerVehicle,              "_Z17FindPlayerVehicleib");   // opcional
    SYM(FlushImmediately,               "_ZN16CPedIntelligence16FlushImmediatelyEb");   // opcional
    SYM(AddInCarAnim,                   "_ZN13CCarEnterExit12AddInCarAnimERK8CVehicleP4CPedb");   // opcional (animacion de sentado)
    SYM(VehIsDriver,                    "_ZNK8CVehicle8IsDriverEPK4CPed");   // opcional
    SYM(VehIsPassenger,                 "_ZNK8CVehicle11IsPassengerEPK4CPed");   // opcional
    SYM(SetModelIndex,                  "_ZN4CPed13SetModelIndexEj");
    SYM(DeleteRwObject,                 "_ZN7CEntity14DeleteRwObjectEv");   // opcional
    SYM(RebuildPlayer,                  "_ZN8CClothes13RebuildPlayerEP10CPlayerPedb");  // opcional
    SYM(GetModelInfoByName,             "_ZN10CModelInfo12GetModelInfoEPKcPi");
    SYM(AddImageToList,                  "_ZN10CStreaming14AddImageToListEPKcb");   // opcional: registra el IMG propio de ARCXLoader (devuelve el slot)
    SYM(CdDirAddItem,                    "_ZN10CDirectory7AddItemERKNS_13DirectoryInfoE");  // opcional: mete una entrada en el CDirectory del juego
    SYM(CdDirFindItem,                   "_ZN10CDirectory8FindItemEPKcRjS2_");      // opcional: busca una entrada en el CDirectory (1 = esta)
    SYM(TxdFindSlot,                     "_ZN9CTxdStore11FindTxdSlotEPKc");         // opcional: busca un .txd por nombre (-1 = no esta)
    SYM(TxdAddSlot,                      "_ZN9CTxdStore10AddTxdSlotEPKcS1_b");      // opcional: registra un .txd propio (devuelve el hueco)
    SYM(InfoSetCdPosnAndSize,            "_ZN14CStreamingInfo16SetCdPosnAndSizeEjj"); // opcional: pone offset/tamano de un .txd en su CStreamingInfo
    SYM(RequestModel,                     "_ZN10CStreaming12RequestModelEii");   // opcional: pide un modelo/txd al streaming (lo carga)
    SYM(RwStreamOpen,                      "_Z12RwStreamOpen12RwStreamType18RwStreamAccessTypePKv"); // opcional: abre un flujo de RenderWare (3.1.19)
    SYM(RwTexDictionaryStreamRead,         "_Z25RwTexDictionaryStreamReadP8RwStream"); // opcional: lee un diccionario de texturas de un flujo (3.1.19)
    SYM(RwStreamClose,                     "_Z13RwStreamCloseP8RwStreamPv");         // opcional: cierra un flujo de RenderWare (3.1.19)
    SYM(TxdAddRef,                         "_ZN9CTxdStore6AddRefEi");                // opcional: sube el contador de referencias de un .txd (3.1.19)
    // LoadCdDirectory NO se llama nunca en partida (en 3.1.7 se cerro el juego
    // dentro de GetEntry): solo se usa su direccion para localizar el objeto
    // CDirectory y el array CStreamingInfo escaneando su codigo.
    void* loadCdDirFn = (void*)aml->GetSym(h, "_ZN10CStreaming15LoadCdDirectoryEPKci");
    LOGI("  %s _ZN10CStreaming15LoadCdDirectoryEPKci (solo para localizar el CDirectory; nunca se llama en partida)", loadCdDirFn ? "ok     " : "FALTA  ");
    loader::InitCdDirectory(loadCdDirFn, CdDirAddItem, CdDirFindItem);
    loader::InitTxd(loadCdDirFn, TxdFindSlot, TxdAddSlot, InfoSetCdPosnAndSize, RequestModel);
    loader::InitQueue((void*)RequestModel);   // localiza la cabeza de la cola de peticiones (3.1.14: para meter los .txd en la cola)
    loader::InitDirectTxd((void*)RwStreamOpen, (void*)RwTexDictionaryStreamRead, (void*)RwStreamClose, (void*)TxdAddRef);   // carga directa del .txd con RenderWare (3.1.19)
    SYM(CFileMgrOpenFile,                "_ZN8CFileMgr8OpenFileEPKcS1_");           // opcional: probar con que ruta abre el juego el IMG propio (0 = no abre)
    SYM(CFileMgrCloseFile,               "_ZN8CFileMgr9CloseFileEy");               // opcional
    *(void**)&ClearTaskDuckSecondary = (void*)aml->GetSym(h, "_ZN16CPedIntelligence22ClearTaskDuckSecondaryEv");
    LOGI("  %s CPedIntelligence::ClearTaskDuckSecondary", ClearTaskDuckSecondary ? "ok" : "FALTA");
    g_pauseUser = (const volatile uint8_t*)aml->GetSym(h, "_ZN6CTimer11m_UserPauseE");
    g_pauseCode = (const volatile uint8_t*)aml->GetSym(h, "_ZN6CTimer11m_CodePauseE");
    LOGI("  %s CTimer::m_UserPause, %s CTimer::m_CodePause", g_pauseUser ? "ok" : "FALTA", g_pauseCode ? "ok" : "FALTA");
    g_scriptSpace = (uint8_t*)aml->GetSym(h, "_ZN11CTheScripts11ScriptSpaceE");
    LOGI("  %s _ZN11CTheScripts11ScriptSpaceE", g_scriptSpace ? "ok     " : "FALTA  ");
    g_cutPtr = (const volatile uint8_t*)aml->GetSym(h, "_ZN12CCutsceneMgr10ms_runningE");
    g_loadPtr = (const volatile uint8_t*)aml->GetSym(h, "_ZN19CGenericGameStorage11ms_bLoadingE");
    LOGI("  %s CCutsceneMgr::ms_running, %s CGenericGameStorage::ms_bLoading", g_cutPtr ? "ok" : "FALTA", g_loadPtr ? "ok" : "FALTA");
    void* process = (void*)aml->GetSym(h, "_ZN5CGame7ProcessEv");
    LOGI("  %s _ZN5CGame7ProcessEv", process ? "ok     " : "FALTA  ");
    #undef SYM

    // El gancho de fopen NO se instala al arrancar: se instala al aplicar la
    // primera skin propia (en ApplySkin), para que el arranque no toque la memoria del juego.
    StartScan();   // en segundo plano: la lista llega por ConsumeScan en el hilo del juego

    if (!RequestSpecialChar || !LoadAllRequestedModels || !HasSpecialCharLoaded ||
        !SetMissionDoesntRequireSpecialChar || !FindPlayerPed || !SetModelIndex || !process) {
        LOGI("faltan simbolos de libGTASA.so: no se puede cambiar la skin");
        return;
    }
    aml->Hook(process, (void*)CGame_Process_hook, (void**)&CGame_Process_orig);
    LOGI("hook puesto en CGame::Process");
    void* initRestart = (void*)aml->GetSym(h, "_ZN5CGame24InitialiseWhenRestartingEv");
    void* shutRestart = (void*)aml->GetSym(h, "_ZN5CGame18ShutDownForRestartEv");
    LOGI("  %s CGame::InitialiseWhenRestarting, %s CGame::ShutDownForRestart", initRestart ? "ok" : "FALTA", shutRestart ? "ok" : "FALTA");
    if (initRestart) aml->Hook(initRestart, (void*)CGame_InitRestart_hook, (void**)&CGame_InitRestart_orig);
    if (shutRestart) aml->Hook(shutRestart, (void*)CGame_ShutRestart_hook, (void**)&CGame_ShutRestart_orig);
}

}  // namespace core
