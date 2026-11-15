// arcx_loader.h - cargador propio de skins de Project ARCX (carpeta ARCXLoader/).
// No depende de ZyZ ModLoader: cada skin vive en su propia subcarpeta,
//   ARCXLoader/<Nombre>/skin.dff (+ su .txd y texturas .png/.jpg sueltas)
// El nombre del modelo es el del .dff sin extension (max. 23 caracteres, como
// en modloader/). Si dos carpetas traen un .dff con el mismo nombre, solo
// entra la primera y la otra queda registrada en el log como duplicada.
//
// COMO CARGA EL JUEGO LOS MODELOS (verificado desensamblando libGTASA.so
// 2.10, que Daniel paso del APK): el juego NO abre los .dff/.txd sueltos por
// fopen/open/openat (cero redirecciones en los logs aunque los ganchos estaban
// instalados): los carga desde archivos .img registrados en su sistema de
// streaming. ZyZ hace exactamente eso: construye su ZYZML\GTA3.IMG y lo
// registra con CStreaming::AddImageToList (su .so lo confirma:
// "_ZN10CStreaming14AddImageToListEPKcb"). PERO solo registrarlo NO basta:
// AddImageToList solo guarda la ruta y abre el archivo; el directorio del
// IMG (que archivo esta en que offset) lo lee CStreaming::LoadCdDirectory,
// que el juego solo ejecuta al cargar el nivel. Como el IMG propio se
// registra ya dentro de la partida, el directorio hay que cargarlo aqui
// mismo. En 3.1.6 se intento llamando a LoadCdDirectory con el indice de
// slot que devolvio AddImageToList, y en 3.1.7 eso CERRO el juego (ver abajo):
// desde 3.1.8 el directorio se carga DIRECTO con CDirectory::AddItem. Sin el
// directorio, en 3.1.4/3.1.5 el juego nunca encontraba 'suk.dff':
// CDirectory::FindItem fallaba y RequestSpecialModel rellenaba la info del
// modelo con basura del stack (no comprueba el resultado de FindItem), asi
// que el pedido nunca cargaba y ademas atascaba el canal de carga y las
// skins de ZyZ que se pedian despues tampoco cargaban. El gancho de
// fopen/open/openat queda solo como diagnostico (por si el juego pidiera
// algun archivo suelto).
//
// LA RUTA DEL IMG (3.1.7, verificado desensamblando CFileMgr::OpenFile
// 0x4d2b74 y CStreaming::LoadCdDirectory 0x5578fc): LoadCdDirectory abre el
// IMG con CFileMgr::OpenFile(ruta, "rb"), y OpenFile ANTEPONE la carpeta base
// del juego a toda ruta que no tenga ':' (es decir, tambien a las rutas
// absolutas de Android). Por eso en 3.1.6 la ruta absoluta
// ".../files/ARCXLoader/arcxml/gta3.img" se convertia en
// "<base>/storage/emulated/0/..." (invalida): OpenFile fallaba,
// LoadCdDirectory salia sin leer nada y el juego nunca encontraba 'suk.dff'
// (y el pedido roto volvia a atascar las skins de ZyZ). ZyZ y el juego usan
// rutas RELATIVAS a la carpeta base ("ZYZML\GTA3.IMG", "TEXDB\GTA3.IMG").
// Por eso RegisterImg ya no usa la ruta absoluta: prueba las candidatas con
// el propio OpenFile del juego y usa la primera que abre (relativa con
// barras invertidas como ZyZ, relativa con barras normales, y la absoluta
// como ultimo recurso). Ademas BuildImg escribe una copia del IMG en la
// carpeta "normal" (Android/data/... en vez de Android_unprotected/data/...),
// por si la carpeta base del juego es esa y no la _unprotected.
//
// 3.1.8: LoadCdDirectory NO se puede llamar en partida (aunque la ruta abra
// bien). En 3.1.7 el juego se cerró justo al llamarla: el crashlog muestra
// SIGSEGV en CModelInfoAccelerator::GetEntry (libGTASA.so + 0x45D3DC), llamado
// desde LoadCdDirectory (+248), que a su vez venía de ARCX. GetEntry, cuando
// el acelerador no está en modo rápido, escribe en su tabla (un array que
// está a NULL fuera de la carga de nivel: el registro X8 era 0 y el fallo
// fue escribir 0xFFFF en NULL + 15339*2 = 0x77D6). El juego solo llama a
// LoadCdDirectory al cargar el nivel, cuando la tabla sí está lista. Por
// eso el directorio del IMG propio se carga DIRECTO, sin pasar por
// LoadCdDirectory: ARCX lee su propio IMG (que él mismo escribió, formato
// VER2) y mete cada entrada en el CDirectory del juego con
// CDirectory::AddItem, que es justo lo que LoadCdDirectory hace para el
// CDirectory (la parte que necesita RequestSpecialModel/FindItem; el relleno
// de CStreamingInfo no hace falta para los personajes especiales, que van
// por la ruta de FindItem). El objeto CDirectory no tiene símbolo público:
// se localiza escaneando el código de LoadCdDirectory (su adrp a la página
// 0x84c000 + ldr #0x180 apunta al global 0x84c180, verificado único en
// LoadCdDirectory y en RequestSpecialModel). Si el escaneo falla, no se
// registra nada (seguro: ZyZ sigue funcionando).
//
// 3.1.9: el global 0x84c180 NO es el CDirectory: es un puntero AL CDirectory
// (LoadCdDirectory y RequestSpecialModel hacen DOBLE indirección para el
// this de AddItem/FindItem). En 3.1.8 se leyó con una sola y se usó un objeto
// con capacidad 0 y conteo 0, así que el directorio nunca se cargó y el
// reintento a cada frame llamó a AddImageToList una y otra vez hasta llenar
// los 36 huecos de la lista de imágenes y agotar los 32 manejadores de
// CdStreamOpen: el juego se cerró en CdStreamRead al entrar a la partida
// (el crashlog muestra texto de ruta "l\gta3.i" en los registros: se leyó
// una ruta como si fuera un puntero). Ahora se hace doble indirección con
// comprobación (capacidad > 0 y conteo entre 0 y capacidad), el slot se
// reserva una sola vez por sesión y tras un fallo se espera ~10 s antes de
// reintentarlo, sin volver a reservar slot.
//
// 3.1.10: LoadCdDirectory TRUNCA el nombre de cada entrada en el primer '.'
// antes de meterla en el CDirectory (desensamblado 0x557bcc-0x557bf8: strchr
// busca '.', escribe 0 ahi y luego decide por la extension: solo "DFF" va
// a AddItem, "TXD" va al CTxdStore). En 3.1.9 ARCX metia el nombre completo
// ("suk.dff") en el CDirectory, asi que FindItem("suk") no encontraba nada
// (compara el nombre entero) y RequestSpecialModel llenaba el CStreamingInfo
// del slot con basura del stack (no comprueba si FindItem encontro algo):
// 'suk' nunca cargaba y el pedido roto atascaba tambien las skins de ZyZ.
// Ahora LoadImgDirectory hace lo mismo que el juego: trunca en '.' y solo
// mete los .dff (los .txd NO van al CDirectory; sus texturas son el
// siguiente paso, registrandolos en el CTxdStore como hace LoadCdDirectory).
//
// 3.1.11: los .txd SI se registran, en el CTxdStore (no en el CDirectory).
// LoadCdDirectory, para un .txd, hace: FindTxdSlot(nombre) y si no existe
// AddTxdSlot(nombre, rutaIMG, false) -> huecoTxd; modelId = huecoTxd +
// 0x4E20; y rellena CStreamingInfo[modelId] (imagen = slot del IMG en +7,
// campo +6 = 0, SetCdPosnAndSize(offset SIN el slot, tamano)). El .dff se
// puso en 3.1.10 pero se veia en blanco porque el .txd no estaba registrado:
// RequestSpecialModel busca el txd con FindTxdSlot(nombre de la skin) y, si
// no lo encuentra, usa el txd por defecto (sin las texturas de la skin).
// La base del array CStreamingInfo es el global 0x84dbd0 (sin simbolo: se
// localiza escaneando LoadCdDirectory con adrp a 0x84d000 + ldr #0xbd0,
// verificado unico en LoadCdDirectory y en RequestSpecialModel, que lo lee
// dos veces). Los campos del CStreamingInfo (20 bytes): +4 campo (0xFFFF al
// pedir un modelo), +6 byte (0), +7 byte indice de imagen, +8 offset en
// sectores, +12 tamano en sectores (lo que GetCdPosnAndSize devuelve como
// "tamano" y comprueba para saber si ya esta puesto).
//
// 3.1.19 (CARGA DIRECTA DEL .TXD, SIN LA COLA): la imagen que mando Daniel
// describe el flujo correcto: abrir el .txd con RwStreamOpen y cargarlo con
// CTxdStore::LoadTxd + AddRef. Pero en este libGTASA.so los LoadTxd del
// juego son stubs (devuelven 1 sin hacer nada: 0x6f9090, 0x6f9098, 0x6f90a0),
// asi que se hace a mano: RwStreamOpen (memoria) + RwTexDictionaryStreamRead
// (funcion real, 0x26cb98, 476 bytes) para crear el diccionario, se guarda
// su puntero en la entrada del CTxdStore (+0, que AddTxdSlot deja en 0) y se
// llama a CTxdStore::AddRef para que no se descargue. El .txd se lee del
// propio IMG (sus datos ya estan ahi) y se carga en cuanto se registra, sin
// pasar por la cola de streaming del juego (que dejaba el .txd en estado 1
// para siempre). Por eso RegisterTxd ya no pide el .txd al streaming ni lo
// mete en la cola.
// 3.1.20: las texturas .png SI se convierten a texturas de RenderWare y se
// cargan directo (LoadPngForSkins, como CustomCharacter): ya no hace falta
// ningun .txd para que la skin se vea con texturas. Los .jpg se inventarian
// y se muestran en el log, pero NO se cargan (RtJPEGImageRead no existe en
// este libGTASA.so).
#pragma once
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <strings.h>   // strncasecmp (POSIX; no viene en <cstring>)
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <dirent.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "skinscan.h"
#include "arcx_common.h"

namespace loader {

static void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsa::LogV("loader", "ARCX", fmt, ap);
    va_end(ap);
}

// ---- escaneo de ARCXLoader/ -------------------------------------------------
// Una carpeta = una skin. Dentro: un .dff (el modelo), un .txd (texturas) y
// .png/.jpg sueltos (se cuentan). Tambien valen .dff sueltos en la raiz.
static void ScanPack(const std::string& packDir, const std::string& packName,
                     std::vector<skinscan::Skin>& out, std::vector<std::string>* rejected) {
    DIR* d = opendir(packDir.c_str());
    if (!d) return;
    std::string dffPath, txdPath;
    int pngs = 0, jpgs = 0;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue;
        std::string full = packDir + "/" + n;
        struct stat st;
        if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (skinscan::EndsWith(n, ".dff")) {
            if (dffPath.empty()) dffPath = full;
        } else if (skinscan::EndsWith(n, ".txd")) {
            if (txdPath.empty()) txdPath = full;
        } else if (skinscan::EndsWith(n, ".png")) {
            pngs++;
        } else if (skinscan::EndsWith(n, ".jpg") || skinscan::EndsWith(n, ".jpeg")) {
            jpgs++;
        }
    }
    closedir(d);
    if (dffPath.empty()) {
        if (pngs + jpgs > 0)
            Log("  carpeta '%s': solo texturas sueltas (%d png, %d jpg), sin .dff: no es una skin", packName.c_str(), pngs, jpgs);
        return;
    }
    std::string base = dffPath.substr(dffPath.find_last_of('/') + 1);
    base = base.substr(0, base.size() - 4);   // sin .dff
    if (base.empty() || base.size() > 23) {
        Log("  carpeta '%s': nombre de modelo '%s' no valido (vacio o >23 caracteres)", packName.c_str(), base.c_str());
        if (rejected) rejected->push_back(dffPath);
        return;
    }
    bool ped = skinscan::IsPedDff(dffPath);
    if (!ped) {
        Log("  carpeta '%s': '%s' sin huesos (no es un ped): se ignora", packName.c_str(), base.c_str());
        if (rejected) rejected->push_back(dffPath);
        return;
    }
    for (auto& s : out)
        if (skinscan::Lower(s.name) == skinscan::Lower(base)) {
            Log("  carpeta '%s': modelo '%s' duplicado (ya existe): se ignora", packName.c_str(), base.c_str());
            if (rejected) rejected->push_back(dffPath);
            return;
        }
    skinscan::Skin sk;
    sk.name = base;
    sk.path = dffPath;
    sk.ped = true;
    sk.src = 1;
    sk.dir = packDir;
    sk.hasTxd = !txdPath.empty();
    sk.txdPath = txdPath;
    sk.pngCount = pngs;
    sk.jpgCount = jpgs;
    out.push_back(sk);
    Log("  + skin propia: %s  [carpeta %s, .txd: %s, png: %d, jpg: %d]", base.c_str(), packName.c_str(),
        sk.hasTxd ? "SI" : "NO (se vera sin texturas)", pngs, jpgs);
}

inline void ScanLoader(const std::string& root, std::vector<skinscan::Skin>& out,
                       std::vector<std::string>* rejected = nullptr) {
    DIR* d = opendir(root.c_str());
    if (!d) { Log("ARCXLoader: no existe %s (se crea al arrancar)", root.c_str()); return; }
    // .dff sueltos en la raiz de ARCXLoader/
    {
        std::vector<std::string> names;
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            std::string full = root + "/" + n;
            struct stat st;
            if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && skinscan::EndsWith(n, ".dff")) names.push_back(full);
        }
        for (auto& f : names) {
            std::string base = f.substr(f.find_last_of('/') + 1);
            base = base.substr(0, base.size() - 4);
            if (base.empty() || base.size() > 23 || !skinscan::IsPedDff(f)) { if (rejected) rejected->push_back(f); continue; }
            bool dup = false;
            for (auto& s : out) if (skinscan::Lower(s.name) == skinscan::Lower(base)) { dup = true; break; }
            if (dup) { if (rejected) rejected->push_back(f); continue; }
            skinscan::Skin sk;
            sk.name = base; sk.path = f; sk.ped = true; sk.src = 1; sk.dir = root;
            out.push_back(sk);
            Log("  + skin propia: %s  [suelta en ARCXLoader/]", base.c_str());
        }
    }
    // subcarpetas: una carpeta = una skin
    rewinddir(d);
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.empty() || n[0] == '.') continue;
        std::string full = root + "/" + n;
        struct stat st;
        if (stat(full.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        ScanPack(full, n, out, rejected);
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const skinscan::Skin& a, const skinscan::Skin& b) { return skinscan::Lower(a.name) < skinscan::Lower(b.name); });
}

// ---- IMG propio (arcxml/gta3.img): el mecanismo real del cargador --------
// Formato IMG v2 (el mismo que usan los .img del juego y el ZYZML\GTA3.IMG
// de ZyZ): cabecera de 8 bytes ("VER2" + numEntradas, little-endian) y una
// entrada de 32 bytes por archivo (offset y tamano en sectores de 2048 bytes,
// nombre de max. 23 caracteres). Los datos van en sectores de 2048 bytes.
// El .txd se registra dos veces: con su nombre real y como <modelo>.txd (el
// juego busca las texturas por el nombre del modelo). El IMG se construye UNA
// sola vez por sesion: si se reescribiera mientras el juego ya lo tiene
// registrado, sus offsets en cache quedarian viejos y leeria basura. Por eso
// las skins propias que se anadan despues necesitan reiniciar el juego.
// (Nombres de modelo de mas de 19 caracteres no caben en el IMG con su .dff:
// esas skins se quedan fuera del IMG y el log lo dice.)
static std::atomic<bool> g_imgBuilt{false};
static std::atomic<bool> g_imgRegistered{false};
static std::string g_imgPath;
static int g_imgSlot = -1;            // slot reservado con AddImageToList (-1 = aun no reservado; se conserva entre reintentos)
static int g_nextDirTryFrame = 0;     // frame a partir del cual se reintenta cargar el directorio (enfriamiento tras un fallo)
static std::string g_resolvedPath;    // ruta del IMG que el juego abre (cacheada: no cambia en la sesion)

static bool ValidateImg(const std::string& path, uint32_t expect) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[4];
    uint32_t count = 0;
    bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "VER2", 4) == 0 &&
              fread(&count, 4, 1, f) == 1 && count == expect;
    struct stat st;
    long fsize = (fstat(fileno(f), &st) == 0) ? (long)st.st_size : -1;
    for (uint32_t i = 0; ok && i < count; i++) {
        uint32_t off = 0, sz = 0;
        char nm[24] = {0};
        if (fread(&off, 4, 1, f) != 1 || fread(&sz, 4, 1, f) != 1 || fread(nm, 1, 24, f) != 24) { ok = false; break; }
        if (nm[0] == '\0' || sz == 0 || (long)(off + sz) * 2048 > fsize) ok = false;
    }
    fclose(f);
    return ok;
}

static bool BuildImg(const std::vector<skinscan::Skin>& skins, const std::string& imgPath) {
    if (g_imgBuilt.load()) {
        static bool said = false;
        if (!said) { said = true; Log("el IMG ya esta construido en esta sesion: las skins propias nuevas se aplican al reiniciar el juego"); }
        return true;
    }
    struct Ent { std::string name, file; uint32_t size = 0; };
    std::vector<Ent> ents;
    for (auto& s : skins) {
        if (s.src != 1) continue;
        std::string dffName = s.path.substr(s.path.find_last_of('/') + 1);
        if (dffName.size() > 23) { Log("  IMG: la skin '%s' tiene el nombre demasiado largo para el IMG (max 19 caracteres): no se incluye", s.name.c_str()); continue; }
        ents.push_back({dffName, s.path, 0});
        if (s.hasTxd) {
            std::string txdName = s.txdPath.substr(s.txdPath.find_last_of('/') + 1);
            if (txdName.size() <= 23) ents.push_back({txdName, s.txdPath, 0});
            std::string alias = s.name + ".txd";
            if (alias.size() <= 23 && skinscan::Lower(alias) != skinscan::Lower(txdName))
                ents.push_back({alias, s.txdPath, 0});
        }
    }
    if (ents.empty()) { Log("sin skins propias: no se construye el IMG"); return false; }
    std::string sig;
    for (auto& e : ents) {
        struct stat st;
        e.size = (stat(e.file.c_str(), &st) == 0) ? (uint32_t)st.st_size : 0;
        if (e.size == 0) { Log("  IMG: no se puede leer %s", e.file.c_str()); return false; }
        sig += e.name + ":" + std::to_string(e.size) + ";";
    }
    static std::string g_lastSig;
    struct stat pst;
    if (sig == g_lastSig && stat(imgPath.c_str(), &pst) == 0) { g_imgBuilt = true; g_imgPath = imgPath; return true; }
    std::string dir = imgPath.substr(0, imgPath.find_last_of('/'));
    mkdir(dir.c_str(), 0777);
    FILE* f = fopen(imgPath.c_str(), "wb");
    if (!f) { Log("no se pudo crear el IMG %s", imgPath.c_str()); return false; }
    uint32_t count = (uint32_t)ents.size();
    uint32_t dirBytes = 8 + count * 32;
    uint32_t dataStart = (dirBytes + 2047) / 2048;   // primer sector de datos
    fwrite("VER2", 1, 4, f);
    fwrite(&count, 4, 1, f);
    uint32_t sector = dataStart;
    std::vector<uint32_t> secs;
    for (auto& e : ents) {
        uint32_t n = (e.size + 2047) / 2048;
        fwrite(&sector, 4, 1, f);
        fwrite(&n, 4, 1, f);
        char nm[24] = {0};
        memcpy(nm, e.name.c_str(), e.name.size());
        fwrite(nm, 1, 24, f);
        secs.push_back(n);
        sector += n;
    }
    long pos = ftell(f);
    for (long i = pos; i < (long)dataStart * 2048; i++) fputc(0, f);
    char buf[8192];
    bool bad = false;
    for (size_t k = 0; k < ents.size() && !bad; k++) {
        FILE* in = fopen(ents[k].file.c_str(), "rb");
        if (!in) { Log("  IMG: no se pudo leer %s", ents[k].file.c_str()); bad = true; break; }
        uint32_t written = 0, n = 0;
        while ((n = (uint32_t)fread(buf, 1, sizeof buf, in)) > 0) { fwrite(buf, 1, n, f); written += n; }
        fclose(in);
        for (uint32_t i = written; i < secs[k] * 2048; i++) fputc(0, f);
    }
    fclose(f);
    if (bad || !ValidateImg(imgPath, count)) {
        Log("el IMG construido no pasa la validacion: no se registra");
        remove(imgPath.c_str());
        return false;
    }
    // Copia en la carpeta "normal" (Android/data/... en vez de
    // Android_unprotected/data/...): si la carpeta base del juego es esa y no
    // la _unprotected, la ruta relativa del IMG solo abre ahi. No es fatal si
    // falla (la ruta relativa se prueba con OpenFile antes de registrar).
    {
        const std::string from = "Android_unprotected", to = "Android";
        size_t at = imgPath.find(from);
        if (at != std::string::npos) {
            std::string alt = imgPath;
            alt.replace(at, from.size(), to);
            std::string adir = alt.substr(0, alt.find_last_of('/'));
            mkdir(adir.c_str(), 0777);
            FILE* in = fopen(imgPath.c_str(), "rb");
            FILE* out = in ? fopen(alt.c_str(), "wb") : nullptr;
            if (in && out) {
                char buf[8192]; size_t n;
                while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
                Log("copia del IMG tambien en %s", alt.c_str());
            } else {
                Log("no se pudo copiar el IMG a %s (no es fatal: se usa la ruta _unprotected)", alt.c_str());
            }
            if (in) fclose(in);
            if (out) fclose(out);
        }
    }
    g_lastSig = sig;
    g_imgPath = imgPath;
    g_imgBuilt = true;
    Log("IMG de ARCXLoader construido: %s (%u archivo(s))", imgPath.c_str(), count);
    return true;
}

// Resuelve con que ruta abre el juego el IMG propio: se prueba cada
// candidata con el propio CFileMgr::OpenFile del juego y se usa la primera
// que abre. La relativa con barras invertidas es la que usan ZyZ y el juego;
// la absoluta es el ultimo recurso (en 3.1.6 se probo que falla: OpenFile le
// antepone la carpeta base y la ruta queda invalida). Devuelve "" si ninguna
// abre: en ese caso no se registra nada (registrar un IMG que el juego no
// puede abrir solo atasca el canal de carga).
static std::string ResolveImgPath(uint64_t (*openFile)(const char*, const char*), void (*closeFile)(uint64_t)) {
    const char* cands[] = {
        "ARCXLoader\\arcxml\\gta3.img",   // relativa, como ZyZ ("ZYZML\GTA3.IMG") y el juego ("TEXDB\GTA3.IMG")
        "ARCXLoader/arcxml/gta3.img",     // relativa, con barras normales
        nullptr,                          // absoluta (g_imgPath)
    };
    if (!openFile) {
        Log("CFileMgr::OpenFile no esta en libGTASA.so: no se puede probar la ruta del IMG, se usa la absoluta como antes");
        return g_imgPath;
    }
    for (auto* c : cands) {
        std::string p = c ? c : g_imgPath;
        uint64_t h = openFile(p.c_str(), "rb");
        if (h) {
            if (closeFile) closeFile(h);
            Log("el juego abre el IMG como '%s'", p.c_str());
            return p;
        }
        Log("  el juego NO abre el IMG como '%s'", p.c_str());
    }
    Log("CFileMgr no abre el IMG con ninguna ruta: el juego no podra leer su directorio (las skins propias no cargaran)");
    return "";
}

// Al cargar una partida nueva el juego reconstruye su CDirectory (Init2):
// las entradas del IMG propio desaparecen y hay que volver a registrarlo.
// Se llama desde OnNewSession; el watchdog lo vuelve a registrar en cuanto
// el jugador esta en partida. Tambien se libera el slot reservado (la lista
// de imagenes se reconstruye al cargar el nivel) y el enfriamiento de
// reintento, para que el registro se vuelva a intentar en la partida nueva.
static void ResetRegistration() { g_imgRegistered = false; g_imgSlot = -1; g_nextDirTryFrame = 0; }

// ---- CDirectory del juego (para cargar el directorio del IMG sin LoadCdDirectory) ----
// El objeto CDirectory no tiene simbolo publico: se localiza escaneando el
// codigo de CStreaming::LoadCdDirectory, que lee su puntero de un global con
// "adrp x?, #0x84c000" + "ldr x?, [x?, #0x180]" (en 2.10, el global 0x84c180).
// OJO (3.1.9): el global NO es el CDirectory, es un puntero AL CDirectory:
// tanto LoadCdDirectory como RequestSpecialModel hacen DOBLE indireccion
// ("ldr x?, [global]" y luego "ldr x0, [x?]" para el this de AddItem/
// FindItem). En 3.1.8 se leia con una sola indireccion y se usaba un objeto
// con capacidad 0 y conteo 0 ("el CDirectory del juego esta lleno (0/0)"),
// asi que el directorio nunca se cargaba y el reintento a cada frame llamaba
// a AddImageToList una y otra vez hasta llenar los 36 huecos de la lista de
// imagenes y agotar los 32 manejadores de CdStreamOpen: el juego se cerro en
// CdStreamRead al intentar entrar a la partida.
static uintptr_t g_cdDirGlobal = 0;   // direccion del global que apunta al CDirectory (0 = no localizado)
static void (*g_cdDirAddItem)(void*, const void*) = nullptr;              // CDirectory::AddItem(this, const DirectoryInfo&)
static int (*g_cdDirFindItem)(void*, const char*, uint32_t*, uint32_t*) = nullptr; // CDirectory::FindItem(this, nombre, &offset, &tamano) -> 1 si lo encuentra

static uintptr_t FindCdDirectoryGlobal(void* loadCdDirectoryFn) {
    if (!loadCdDirectoryFn) return 0;
    const uint32_t* code = (const uint32_t*)loadCdDirectoryFn;
    const uintptr_t base = (uintptr_t)loadCdDirectoryFn;
    for (int i = 0; i < 0x13C; i++) {   // LoadCdDirectory mide ~0x500 bytes
        uint32_t w = code[i];
        if ((w & 0x9F000000u) != 0x90000000u) continue;   // adrp: bit31=1, bit28=0
        int32_t imm = (int32_t)(((w >> 5) & 0x7FFFF) << 2) | (int32_t)((w >> 29) & 0x3);
        if (imm & 0x100000) imm -= 0x200000;   // extension de signo (21 bits)
        uintptr_t page = (base + (uintptr_t)i * 4) & ~(uintptr_t)0xFFF;
        uintptr_t target = page + ((uintptr_t)imm << 12);
        int rd = (int)(w & 0x1F);
        for (int j = 1; j <= 4; j++) {
            uint32_t w2 = code[i + j];
            if ((w2 & 0xFFC00000u) != 0xF9400000u) continue;   // ldr (offset sin signo, 64 bits)
            uint32_t imm12 = (w2 >> 10) & 0xFFF;
            int rn = (int)((w2 >> 5) & 0x1F);
            if (imm12 * 8 == 0x180 && rn == rd) return target + 0x180;
        }
    }
    Log("no se pudo localizar el CDirectory del juego en el codigo de LoadCdDirectory: las skins propias no se registraran");
    return 0;
}

// Resuelve el objeto CDirectory con doble indireccion y comprueba que sea
// valido (capacidad > 0 y conteo entre 0 y capacidad). Se resuelve en cada
// intento de registro: al arrancar el juego puede no estar construido aun.
static void* GetCdDirectory() {
    if (!g_cdDirGlobal) return nullptr;
    void* holder = *(void**)g_cdDirGlobal;
    if (!holder) return nullptr;
    void* cdDir = *(void**)holder;
    if (!cdDir) return nullptr;
    int32_t cap = *(int32_t*)((char*)cdDir + 8);
    int32_t cnt = *(int32_t*)((char*)cdDir + 12);
    if (cap <= 0 || cnt < 0 || cnt > cap) return nullptr;   // no parece un CDirectory valido
    return cdDir;
}

static void InitCdDirectory(void* loadCdDirectoryFn, void (*addItem)(void*, const void*), int (*findItem)(void*, const char*, uint32_t*, uint32_t*)) {
    g_cdDirAddItem = addItem;
    g_cdDirFindItem = findItem;
    g_cdDirGlobal = FindCdDirectoryGlobal(loadCdDirectoryFn);
    if (g_cdDirGlobal) {
        void* cd = GetCdDirectory();
        if (cd) Log("CDirectory del juego localizado en %p (global %p)", cd, (void*)g_cdDirGlobal);
        else Log("CDirectory del juego aun no listo (global %p): se reintentara al registrar el IMG", (void*)g_cdDirGlobal);
    }
}

// ---- CTxdStore + CStreamingInfo (para registrar los .txd propios) ----
// El .txd no va al CDirectory: va al CTxdStore (por nombre) y su posicion en
// el IMG va al CStreamingInfo[modelId] (modelId = huecoTxd + 0x4E20), para
// que el sistema de streaming pueda leerlo. Todo es operacion de registro
// (sin tocar el acelerador ni cargar niveles): seguro en partida.
static uintptr_t g_streamInfoGlobal = 0;   // direccion del global con la base del array CStreamingInfo (0 = no localizado)
static int (*g_txdFindSlot)(const char*) = nullptr;                        // CTxdStore::FindTxdSlot(nombre) -> hueco o -1
static int (*g_txdAddSlot)(const char*, const char*, bool) = nullptr;     // CTxdStore::AddTxdSlot(nombre, rutaIMG, false) -> hueco
static void (*g_infoSetCdPosnAndSize)(void*, uint32_t, uint32_t) = nullptr; // CStreamingInfo::SetCdPosnAndSize(this, offset, tamano)
static void (*g_requestModel)(int, int) = nullptr;                          // CStreaming::RequestModel(modelo, flags) (opcional: pide un modelo/txd al streaming)
static uintptr_t g_queueHeadGlobal = 0;   // direccion del global con la cabeza de la cola de peticiones (0 = no localizado)
static uintptr_t g_reqCountGlobal = 0;    // direccion del global con el contador de modelos pedidos (0 = no localizado)

// Carga DIRECTA del .txd con RenderWare (3.1.19): en vez de pedir el .txd
// al streaming del juego (que lo dejaba en estado 1 para siempre), se lee
// el archivo .txd y se crea su diccionario de texturas con
// RwTexDictionaryStreamRead. Los LoadTxd del juego son stubs (devuelven 1
// sin hacer nada), asi que el diccionario se guarda a mano en la entrada
// del CTxdStore (+0, que AddTxdSlot deja en 0) y se llama a AddRef para que
// no se descargue. Tipos de RenderWare (valores estandar del SDK):
// rwSTREAMMEMORY = 3 (flujo desde memoria), rwSTREAMREAD = 1 (solo lectura).
struct RwMemory { void* start; uint32_t length; };   // lo que RwStreamOpen(rwSTREAMMEMORY) espera como tercer argumento
static void* (*g_rwStreamOpen)(int, int, const void*) = nullptr;        // RwStreamOpen(tipo, acceso, datos) -> RwStream* o null
static void* (*g_rwTexDictionaryStreamRead)(void*) = nullptr;            // RwTexDictionaryStreamRead(stream) -> RwTexDictionary* o null
static void (*g_rwStreamClose)(void*, void*) = nullptr;                  // RwStreamClose(stream, datos) (el segundo argumento es null)
static void (*g_txdAddRef)(int) = nullptr;                               // CTxdStore::AddRef(hueco) (sube el contador de referencias)
static uintptr_t g_txdStoreGlobal = 0;   // direccion del global con la tabla del CTxdStore (0 = no localizado)
static std::vector<void*> g_txdBuffers;  // buffers de .txd leidos: se guardan para no liberarlos (el diccionario puede referenciarlos)

static uintptr_t FindStreamingInfoGlobal(void* loadCdDirectoryFn) {
    if (!loadCdDirectoryFn) return 0;
    const uint32_t* code = (const uint32_t*)loadCdDirectoryFn;
    const uintptr_t base = (uintptr_t)loadCdDirectoryFn;
    for (int i = 0; i < 0x13C; i++) {   // LoadCdDirectory mide ~0x500 bytes
        uint32_t w = code[i];
        if ((w & 0x9F000000u) != 0x90000000u) continue;   // adrp: bit31=1, bit28=0
        int32_t imm = (int32_t)(((w >> 5) & 0x7FFFF) << 2) | (int32_t)((w >> 29) & 0x3);
        if (imm & 0x100000) imm -= 0x200000;   // extension de signo (21 bits)
        uintptr_t page = (base + (uintptr_t)i * 4) & ~(uintptr_t)0xFFF;
        uintptr_t target = page + ((uintptr_t)imm << 12);
        // OJO: NO se comprueba que target sea 0x84d000: el .so es independiente
        // de posicion y en el juego carga en una base distinta de 0, asi que
        // target es base+0x84d000. El par adrp + ldr #0xbd0 es UNICO en
        // LoadCdDirectory (verificado), asi que basta con el.
        int rd = (int)(w & 0x1F);
        for (int j = 1; j <= 8; j++) {   // el ldr esta hasta 8 instrucciones despues del adrp
            uint32_t w2 = code[i + j];
            if ((w2 & 0xFFC00000u) != 0xF9400000u) continue;   // ldr (offset sin signo, 64 bits)
            uint32_t imm12 = (w2 >> 10) & 0xFFF;
            int rn = (int)((w2 >> 5) & 0x1F);
            if (imm12 * 8 == 0xbd0 && rn == rd) return target + 0xbd0;   // global 0x84dbd0
        }
    }
    Log("no se pudo localizar el array CStreamingInfo en el codigo de LoadCdDirectory: los .txd propios no tendran texturas");
    return 0;
}

static void* GetStreamingInfoArray() {
    if (!g_streamInfoGlobal) return nullptr;
    void* arr = *(void**)g_streamInfoGlobal;
    return arr;   // la base del array CStreamingInfo (elementos de 20 bytes)
}

static void InitTxd(void* loadCdDirectoryFn, int (*findSlot)(const char*), int (*addSlot)(const char*, const char*, bool), void (*setPosnAndSize)(void*, uint32_t, uint32_t), void (*requestModel)(int, int)) {
    g_txdFindSlot = findSlot;
    g_txdAddSlot = addSlot;
    g_infoSetCdPosnAndSize = setPosnAndSize;
    g_requestModel = requestModel;
    g_streamInfoGlobal = FindStreamingInfoGlobal(loadCdDirectoryFn);
    if (g_streamInfoGlobal) {
        void* arr = GetStreamingInfoArray();
        if (arr) Log("array CStreamingInfo localizado en %p (global %p)", arr, (void*)g_streamInfoGlobal);
        else Log("array CStreamingInfo aun no listo (global %p): se reintentara al registrar el IMG", (void*)g_streamInfoGlobal);
    }
}

// Localiza el global con la tabla del CTxdStore (3.1.19) escaneando el codigo
// de CTxdStore::AddRef: busca un adrp seguido (hasta 4 instrucciones despues)
// por un ldr de 64 bits con desplazamiento 0x4e8 y el mismo registro destino.
// En el juego ese global es 0x8514e8 (la tabla se alcanza con triple
// desreferencia: *(*(*(0x8514e8))) = base de las entradas de 64 bytes).
static uintptr_t FindTxdStoreGlobal(void* addRefFn) {
    if (!addRefFn) return 0;
    const uint32_t* code = (const uint32_t*)addRefFn;
    const uintptr_t base = (uintptr_t)addRefFn;
    for (int i = 0; i < 0x10; i++) {   // AddRef mide 40 bytes (10 palabras)
        uint32_t w = code[i];
        if ((w & 0x9F000000u) != 0x90000000u) continue;   // adrp
        int32_t imm = (int32_t)(((w >> 5) & 0x7FFFF) << 2) | (int32_t)((w >> 29) & 0x3);
        if (imm & 0x100000) imm -= 0x200000;
        uintptr_t page = (base + (uintptr_t)i * 4) & ~(uintptr_t)0xFFF;
        uintptr_t target = page + ((uintptr_t)imm << 12);
        int rd = (int)(w & 0x1F);
        for (int j = 1; j <= 4; j++) {
            uint32_t w2 = code[i + j];
            if ((w2 & 0xFFC00000u) != 0xF9400000u) continue;   // ldr (offset sin signo, 64 bits)
            uint32_t imm12 = (w2 >> 10) & 0xFFF;
            int rn = (int)((w2 >> 5) & 0x1F);
            if (imm12 * 8 == 0x4e8 && rn == rd) return target + 0x4e8;   // global 0x8514e8
        }
    }
    Log("no se pudo localizar la tabla del CTxdStore en el codigo de AddRef: los .txd propios no se cargaran directos");
    return 0;
}

// Devuelve la base de las entradas del CTxdStore (64 bytes por hueco) o null.
// La tabla se alcanza con triple desreferencia desde el global 0x8514e8.
static void* GetTxdEntriesBase() {
    if (!g_txdStoreGlobal) return nullptr;
    void* p1 = *(void**)g_txdStoreGlobal;
    if (!p1) return nullptr;
    void* p2 = *(void**)p1;
    if (!p2) return nullptr;
    void* entries = *(void**)p2;
    return entries;
}

// Guarda las funciones de RenderWare para la carga directa del .txd (3.1.19)
// y localiza el global con la tabla del CTxdStore.
static void InitDirectTxd(void* rwStreamOpen, void* rwTexDictionaryStreamRead, void* rwStreamClose, void* txdAddRef) {
    g_rwStreamOpen = (void* (*)(int, int, const void*))rwStreamOpen;
    g_rwTexDictionaryStreamRead = (void* (*)(void*))rwTexDictionaryStreamRead;
    g_rwStreamClose = (void (*)(void*, void*))rwStreamClose;
    g_txdAddRef = (void (*)(int))txdAddRef;
    g_txdStoreGlobal = FindTxdStoreGlobal(txdAddRef);
    if (g_txdStoreGlobal) Log("tabla del CTxdStore localizada (global %p)", (void*)g_txdStoreGlobal);
}

// Dice si el .txd de una skin ya tiene su diccionario de texturas cargado
// (3.1.19): la entrada del CTxdStore tiene el puntero al diccionario en +0
// (AddTxdSlot lo deja en 0; LoadTxdDirect lo pone). Si no se puede comprobar,
// devuelve false.
static bool TxdDictLoaded(const char* name) {
    if (!g_txdFindSlot) return false;
    int txdSlot = g_txdFindSlot(name);
    if (txdSlot < 0) return false;
    void* entries = GetTxdEntriesBase();
    if (!entries) return false;
    void* entry = (char*)entries + (uintptr_t)txdSlot * 0x40;
    return *(void**)entry != nullptr;
}

// Carga el .txd de una skin DIRECTO desde sus datos en memoria (3.1.19):
// crea un flujo de RenderWare desde la memoria, lee el diccionario de
// texturas con RwTexDictionaryStreamRead, lo guarda en la entrada del
// CTxdStore (+0) y llama a AddRef para que el juego no lo descargue. Los
// datos (data) se guardan y NO se liberan: el diccionario puede
// referenciarlos. Devuelve true si el diccionario quedo cargado.
static bool LoadTxdDirect(const char* name, void* data, size_t dataSize) {
    if (!g_txdFindSlot || !g_rwStreamOpen || !g_rwTexDictionaryStreamRead || !g_rwStreamClose || !g_txdAddRef) {
        Log("  sin funciones de RenderWare: el .txd '%s' no se puede cargar directo", name);
        return false;
    }
    int txdSlot = g_txdFindSlot(name);
    if (txdSlot < 0) { Log("  txd de '%s': no esta registrado (FindTxdSlot = -1)", name); return false; }
    void* entries = GetTxdEntriesBase();
    if (!entries) { Log("  sin tabla del CTxdStore: el .txd '%s' no se puede cargar directo", name); return false; }
    void* entry = (char*)entries + (uintptr_t)txdSlot * 0x40;
    if (*(void**)entry) { Log("  txd de '%s': su diccionario ya estaba cargado (hueco %d)", name, txdSlot); return true; }
    RwMemory mem = { data, (uint32_t)dataSize };
    void* stream = g_rwStreamOpen(3, 1, &mem);   // rwSTREAMMEMORY, rwSTREAMREAD
    if (!stream) { Log("  RwStreamOpen no pudo abrir el .txd '%s' desde memoria", name); return false; }
    void* dict = g_rwTexDictionaryStreamRead(stream);
    g_rwStreamClose(stream, nullptr);
    if (!dict) { Log("  RwTexDictionaryStreamRead no pudo leer el .txd '%s' (puede estar en formato DXT de PC, no compatible con Android)", name); return false; }
    *(void**)entry = dict;   // la entrada del CTxdStore apunta al diccionario
    g_txdAddRef(txdSlot);    // que el juego no lo descargue en medio de la partida
    g_txdBuffers.push_back(data);   // el buffer se queda: el diccionario puede referenciarlo
    Log("  txd de '%s' CARGADO directo (hueco %d, diccionario %p): sus texturas ya estan en el juego", name, txdSlot, dict);
    return true;
}

// ---- Texturas PNG directas (3.1.20, como CustomCharacter) ----
// En vez de depender del .txd (que el juego no lee si es DXT1, y que ni en
// sin comprimir quiso leer con RwTexDictionaryStreamRead: la v1 y la v2 del
// .txd convertido devolvieron null en 3.1.19), las texturas .png de la
// carpeta de la skin se cargan DIRECTO como texturas de RenderWare, sin
// pasar por ningun .txd ni por el streaming del juego. Es lo que hace
// CustomCharacter (el plugin de otro modder que Daniel paso, verificado
// desensamblando libCustomCharacter64.so): RtPNGImageRead lee el PNG a una
// RwImage, se busca su formato de raster, se crea un RwRaster y se copia la
// imagen, se crea un RwTexture con ese raster y se le pone el nombre de la
// textura (el nombre del PNG sin extension, que debe coincidir con el nombre
// de textura que pide el .dff: p. ej. "0400face.png" para la textura
// "0400face"). Todas las texturas de la skin van a un RwTexDictionary nuevo,
// que se registra en el CTxdStore bajo el nombre de la skin (FindTxdSlot /
// AddTxdSlot + entrada +0 = el diccionario + AddRef). Cuando el juego aplica
// la skin, usa ese diccionario para sus texturas. Si la entrada ya tenia un
// diccionario (el .txd cargo bien con LoadTxdDirect), las PNG no lo
// sobrescriben: el .txd manda. Los .jpg NO se cargan: RtJPEGImageRead no
// existe en este libGTASA.so (solo esta RtPNGImageRead).
static void* (*g_rtPngImageRead)(const char*) = nullptr;              // RtPNGImageRead(ruta) -> RwImage* o null
static void* (*g_rwImageFindRasterFormat)(void*, int, int*, int*, int*, int*) = nullptr; // RwImageFindRasterFormat(imagen, tipo, &a, &b, &c, &d) -> formato o null
static void* (*g_rwRasterCreate)(int, int, int, int) = nullptr;       // RwRasterCreate(a, b, c, d) -> RwRaster* o null
static void* (*g_rwRasterSetFromImage)(void*, void*) = nullptr;        // RwRasterSetFromImage(raster, imagen) -> raster o null
static void* (*g_rwTextureCreate)(void*) = nullptr;                    // RwTextureCreate(raster) -> RwTexture* o null
static void (*g_rwTextureSetName)(void*, const char*) = nullptr;       // RwTextureSetName(textura, nombre)
static void* (*g_rwTexDictionaryCreate)() = nullptr;                   // RwTexDictionaryCreate() -> diccionario o null
static void* (*g_rwTexDictionaryAddTexture)(void*, void*) = nullptr;   // RwTexDictionaryAddTexture(diccionario, textura) -> textura o null
static void (*g_rwImageDestroy)(void*) = nullptr;                      // RwImageDestroy(imagen)
static void (*g_rwRasterDestroy)(void*) = nullptr;                     // RwRasterDestroy(raster)
static std::map<std::string, void*> g_pngDicts;   // nombre de skin (minusculas) -> diccionario PNG ya creado (se re-registra si la sesion lo resetea)

static void InitPng(void* rtPngImageRead, void* rwImageFindRasterFormat, void* rwRasterCreate, void* rwRasterSetFromImage, void* rwTextureCreate, void* rwTextureSetName, void* rwTexDictionaryCreate, void* rwTexDictionaryAddTexture, void* rwImageDestroy, void* rwRasterDestroy) {
    g_rtPngImageRead = (void* (*)(const char*))rtPngImageRead;
    g_rwImageFindRasterFormat = (void* (*)(void*, int, int*, int*, int*, int*))rwImageFindRasterFormat;
    g_rwRasterCreate = (void* (*)(int, int, int, int))rwRasterCreate;
    g_rwRasterSetFromImage = (void* (*)(void*, void*))rwRasterSetFromImage;
    g_rwTextureCreate = (void* (*)(void*))rwTextureCreate;
    g_rwTextureSetName = (void (*)(void*, const char*))rwTextureSetName;
    g_rwTexDictionaryCreate = (void* (*)())rwTexDictionaryCreate;
    g_rwTexDictionaryAddTexture = (void* (*)(void*, void*))rwTexDictionaryAddTexture;
    g_rwImageDestroy = (void (*)(void*))rwImageDestroy;
    g_rwRasterDestroy = (void (*)(void*))rwRasterDestroy;
    if (g_rtPngImageRead && g_rwTexDictionaryCreate)
        Log("carga directa de texturas PNG lista (RtPNGImageRead + RwTexDictionaryCreate)");
    else
        Log("sin RtPNGImageRead o RwTexDictionaryCreate: las texturas PNG no se cargaran (las skins se veran sin texturas si su .txd no carga)");
}

// Formas de ruta para RtPNGImageRead (3.1.21): el flujo de archivos de
// RenderWare antepone una carpeta base a la ruta (como CFileMgr::OpenFile y
// OS_FileOpen: verificado desensamblando OS_FileOpen 0x31b7cc, que antepone
// la base de *(0x84b268)), asi que la ruta ABSOLUTA no abre (la base queda
// pegada delante: "<base>/storage/..."). Por eso en 3.1.20 las 21 PNG
// fallaron aunque el archivo existe y se lee bien con fopen/opendir. Se
// prueban estas formas, en orden, y la primera que funciona se recuerda para
// todas las PNG de la sesion:
//   0 = relativa a files/ ("ARCXLoader/skinssuk/0400face.png")
//   1 = con /files/ delante ("/files/ARCXLoader/skinssuk/0400face.png", como
//       pone el readme de CustomCharacter: "/files/CustomCharacter/...")
//   2 = absoluta (por si el flujo usara fopen directo)
static int g_pngPathStyle = -1;   // -1 = aun no se sabe cual funciona
static std::string PngPathForStyle(const std::string& absPath, int style) {
    if (style == 2) return absPath;
    size_t at = absPath.find("/files/");
    if (at == std::string::npos) return absPath;   // sin "/files/": no hay forma relativa
    if (style == 0) return absPath.substr(at + 7);          // despues de "/files/"
    return absPath.substr(at);                               // desde "/files/"
}

// Carga un PNG como textura de RenderWare (3.1.20), siguiendo el flujo de
// CustomCharacter (verificado desensamblando libCustomCharacter64.so):
// RtPNGImageRead -> RwImageFindRasterFormat(imagen, 4, &a, &b, &c, &d) ->
// RwRasterCreate(b, a, d, c) -> RwRasterSetFromImage(raster, imagen) ->
// RwTextureCreate(raster) -> RwTextureSetName(textura, nombre). Devuelve la
// textura o null. La imagen se destruye tras copiarla al raster; el raster
// se queda (la textura lo usa, no se destruye).
static void* LoadPngTexture(const std::string& path, const std::string& texName) {
    if (!g_rtPngImageRead || !g_rwImageFindRasterFormat || !g_rwRasterCreate || !g_rwRasterSetFromImage || !g_rwTextureCreate || !g_rwTextureSetName) return nullptr;
    // Probar las formas de ruta (3.1.21). Si ya se sabe cual funciona, solo esa.
    int styles[3] = {0, 1, 2};
    int nStyles = 3;
    if (g_pngPathStyle >= 0) { styles[0] = g_pngPathStyle; nStyles = 1; }
    void* image = nullptr;
    for (int i = 0; i < nStyles && !image; i++) {
        std::string p = PngPathForStyle(path, styles[i]);
        image = g_rtPngImageRead(p.c_str());
        if (image && g_pngPathStyle < 0) {
            g_pngPathStyle = styles[i];
            Log("    PNG: la ruta que abre el juego es la forma %d ('%s')", styles[i], p.c_str());
        }
    }
    if (!image) { Log("    PNG '%s': RtPNGImageRead no pudo leerlo (ninguna forma de ruta abrio el archivo)", path.c_str()); return nullptr; }
    int a = 0, b = 0, c = 0, d = 0;
    void* fmt = g_rwImageFindRasterFormat(image, 4, &a, &b, &c, &d);
    if (!fmt) { Log("    PNG '%s': RwImageFindRasterFormat no encontro formato", path.c_str()); if (g_rwImageDestroy) g_rwImageDestroy(image); return nullptr; }
    if (b < 1 || a < 1) { Log("    PNG '%s': dimensiones invalidas (%d x %d)", path.c_str(), a, b); if (g_rwImageDestroy) g_rwImageDestroy(image); return nullptr; }
    void* raster = g_rwRasterCreate(b, a, d, c);
    if (!raster) { Log("    PNG '%s': RwRasterCreate fallo", path.c_str()); if (g_rwImageDestroy) g_rwImageDestroy(image); return nullptr; }
    if (!g_rwRasterSetFromImage(raster, image)) { Log("    PNG '%s': RwRasterSetFromImage fallo", path.c_str()); if (g_rwRasterDestroy) g_rwRasterDestroy(raster); if (g_rwImageDestroy) g_rwImageDestroy(image); return nullptr; }
    void* tex = g_rwTextureCreate(raster);
    if (!tex) { Log("    PNG '%s': RwTextureCreate fallo", path.c_str()); if (g_rwRasterDestroy) g_rwRasterDestroy(raster); if (g_rwImageDestroy) g_rwImageDestroy(image); return nullptr; }
    g_rwTextureSetName(tex, texName.c_str());
    if (g_rwImageDestroy) g_rwImageDestroy(image);
    return tex;
}

// Carga las texturas PNG de las skins propias (3.1.20). Para cada skin propia
// con .png en su carpeta: si su entrada del CTxdStore ya tiene un diccionario
// (el .txd cargo bien con LoadTxdDirect), las PNG no lo tocan: el .txd manda.
// Si no tiene diccionario (el .txd fallo o la skin no tiene .txd), se crea un
// diccionario nuevo con todas sus PNG (nombre = el archivo sin extension) y
// se registra en el CTxdStore bajo el nombre de la skin (FindTxdSlot /
// AddTxdSlot + entrada +0 = el diccionario + AddRef). El diccionario creado
// se guarda en g_pngDicts: si al cargar una partida nueva la entrada se
// resetea a null, se vuelve a registrar el mismo diccionario (sin recargar
// los PNG). Se llama desde el watchdog del core cuando el IMG ya esta
// registrado. Si una skin falla (ningun PNG cargo), se anota en
// g_pngFailFrame y no se reintenta hasta pasados ~10 s (600 frames): en
// 3.1.20 el reintento a cada frame lleno el log con 7371 lineas de error.
// Devuelve cuantas skins cargaron o re-registraron sus PNG en esta llamada.
static std::map<std::string, int> g_pngFailFrame;   // nombre de skin (minusculas) -> frame del ultimo fallo (enfriamiento de ~10 s)
static int LoadPngForSkins(const std::vector<skinscan::Skin>& skins, int frame) {
    if (!g_rtPngImageRead || !g_rwTexDictionaryCreate || !g_rwTexDictionaryAddTexture || !g_txdFindSlot || !g_txdAddSlot || !g_txdAddRef) return 0;
    int done = 0;
    for (auto& s : skins) {
        if (s.src != 1) continue;
        if (s.pngCount <= 0) continue;
        std::string low = skinscan::Lower(s.name);
        auto failIt = g_pngFailFrame.find(low);
        if (failIt != g_pngFailFrame.end() && frame - failIt->second < 600) continue;   // enfriamiento tras un fallo: no se reintenta a cada frame
        int txdSlot = g_txdFindSlot(s.name.c_str());
        if (txdSlot < 0) {
            const char* p = !g_resolvedPath.empty() ? g_resolvedPath.c_str() : g_imgPath.c_str();
            txdSlot = g_txdAddSlot(s.name.c_str(), p, false);
            if (txdSlot < 0) { Log("  PNG de '%s': no se pudo registrar en el CTxdStore", s.name.c_str()); continue; }
        }
        void* entries = GetTxdEntriesBase();
        if (!entries) continue;
        void* entry = (char*)entries + (uintptr_t)txdSlot * 0x40;
        void* curDict = *(void**)entry;
        auto it = g_pngDicts.find(low);
        if (it != g_pngDicts.end()) {
            if (curDict == it->second) continue;   // el diccionario PNG ya esta registrado
            if (curDict) { Log("  PNG de '%s': la entrada ya tiene otro diccionario (hueco %d): las PNG no lo sobrescriben", s.name.c_str(), txdSlot); continue; }
            *(void**)entry = it->second;   // la sesion reseteo la entrada: se vuelve a registrar el mismo diccionario
            g_txdAddRef(txdSlot);
            Log("  diccionario PNG de '%s' re-registrado (hueco %d)", s.name.c_str(), txdSlot);
            done++;
            continue;
        }
        if (curDict) { Log("  PNG de '%s': el .txd ya tiene diccionario (hueco %d): las PNG no lo sobrescriben", s.name.c_str(), txdSlot); g_pngDicts[low] = curDict; continue; }
        // Escanear la carpeta de la skin por .png
        std::vector<std::pair<std::string, std::string>> pngs;   // (nombre de textura, ruta)
        DIR* d = opendir(s.dir.c_str());
        if (!d) continue;
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.empty() || n[0] == '.') continue;
            if (!skinscan::EndsWith(n, ".png")) continue;
            std::string full = s.dir + "/" + n;
            struct stat st;
            if (stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
            std::string texName = n.substr(0, n.size() - 4);
            if (texName.empty() || texName.size() > 32) continue;
            pngs.push_back({texName, full});
        }
        closedir(d);
        if (pngs.empty()) continue;
        void* dict = g_rwTexDictionaryCreate();
        if (!dict) { Log("  PNG de '%s': RwTexDictionaryCreate fallo", s.name.c_str()); continue; }
        int ok = 0;
        for (auto& p : pngs) {
            void* tex = LoadPngTexture(p.second, p.first);
            if (!tex) continue;
            if (!g_rwTexDictionaryAddTexture(dict, tex)) { Log("    PNG '%s': no se pudo meter en el diccionario", p.first.c_str()); continue; }
            ok++;
        }
        if (ok == 0) { Log("  PNG de '%s': ningun PNG cargo (se queda sin texturas): se reintentara en ~10 s", s.name.c_str()); g_pngFailFrame[low] = frame; continue; }
        *(void**)entry = dict;
        g_txdAddRef(txdSlot);
        g_pngDicts[low] = dict;
        g_pngFailFrame.erase(low);   // si antes fallo, ya no esta en enfriamiento
        Log("  texturas PNG de '%s' CARGADAS (%d textura(s), hueco %d): la skin ya se ve con texturas", s.name.c_str(), ok, txdSlot);
        done++;
    }
    return done;
}

// Localiza un global del juego escaneando el codigo de CStreaming::RequestModel
// (3.1.14): busca un adrp a la pagina dada seguido (hasta 4 instrucciones
// despues) por un ldr de 64 bits con el desplazamiento dado y el mismo
// registro destino. Devuelve la direccion del global o 0. Igual que
// FindCdDirectoryGlobal/FindStreamingInfoGlobal pero para RequestModel (no se
// comprueba la pagina absoluta: el .so carga en una base distinta de 0 y el
// par adrp + ldr es unico en RequestModel para cada desplazamiento).
static uintptr_t FindRequestModelGlobal(void* requestModelFn, uint32_t ldrImm) {
    if (!requestModelFn) return 0;
    const uint32_t* code = (const uint32_t*)requestModelFn;
    const uintptr_t base = (uintptr_t)requestModelFn;
    for (int i = 0; i < 0xB0; i++) {   // RequestModel mide 704 bytes = 176 palabras
        uint32_t w = code[i];
        if ((w & 0x9F000000u) != 0x90000000u) continue;   // adrp
        int32_t imm = (int32_t)(((w >> 5) & 0x7FFFF) << 2) | (int32_t)((w >> 29) & 0x3);
        if (imm & 0x100000) imm -= 0x200000;
        uintptr_t page = (base + (uintptr_t)i * 4) & ~(uintptr_t)0xFFF;
        uintptr_t target = page + ((uintptr_t)imm << 12);
        int rd = (int)(w & 0x1F);
        for (int j = 1; j <= 4; j++) {
            uint32_t w2 = code[i + j];
            if ((w2 & 0xFFC00000u) != 0xF9400000u) continue;   // ldr (offset sin signo, 64 bits)
            uint32_t imm12 = (w2 >> 10) & 0xFFF;
            int rn = (int)((w2 >> 5) & 0x1F);
            if (imm12 * 8 == ldrImm && rn == rd) return target + ldrImm;
        }
    }
    return 0;
}

// Localiza los globales de la cola de peticiones escaneando RequestModel
// (3.1.14): la cabeza de la cola (ldr #0xdf0, pagina 0x84a000 -> 0x84adf0) y
// el contador de modelos pedidos (ldr #0x580, pagina 0x84a000 -> 0x84a580).
// Ambos pares son unicos en RequestModel (verificado contra el libGTASA.so).
static void InitQueue(void* requestModelFn) {
    g_queueHeadGlobal = FindRequestModelGlobal(requestModelFn, 0xdf0);
    g_reqCountGlobal = FindRequestModelGlobal(requestModelFn, 0x580);
    if (g_queueHeadGlobal) Log("cola de peticiones del juego localizada (global %p)", (void*)g_queueHeadGlobal);
    else Log("no se pudo localizar la cola de peticiones: los .txd propios puede que no carguen");
}

// Mete un modelo en la cola de peticiones del juego (3.1.14), replicando la
// insercion al principio de la lista que hace CStreaming::RequestModel para
// los modelos normales (0x394bec-0x394c80). La cola es doblemente enlazada
// con indices de modelo (int16): +0 siguiente, +2 anterior; la cabeza es un
// centinela cuyo +0 es el primer modelo. Se pone el estado a 2 (en la cola).
// CORRECCION 3.1.15: la cabeza es *(*(0x84adf0)) (DOBLE desreferencia: el
// global guarda un puntero a la cabeza, y la cabeza es el CStreamingInfo
// centinela). En 3.1.14 se uso una sola desreferencia, asi que se escribio el
// id del modelo encima del puntero de la cabeza y el juego se cerro al cargar
// una partida dentro de GetNextFileOnCd (fault 0x0). Ahora se valida que la
// cabeza este dentro del array y que los indices sean sanos antes de tocar
// nada; si algo no cuadra, no se mete en la cola y se registra el motivo.
// Devuelve false si no se localizo la cabeza o el array.
static bool QueueModel(int modelId) {
    if (!g_queueHeadGlobal) return false;
    void* arr = GetStreamingInfoArray();
    if (!arr) return false;
    void* headPtr = *(void**)g_queueHeadGlobal;   // puntero a la cabeza (global 0x84adf0)
    if (!headPtr) { Log("  cola de carga: el puntero a la cabeza es nulo"); return false; }
    void* head = *(void**)headPtr;                // la cabeza: el CStreamingInfo centinela (doble desreferencia)
    if (!head) { Log("  cola de carga: la cabeza es nula"); return false; }
    // La cabeza debe ser una entrada del array CStreamingInfo (su indice es
    // (cabeza - base) / 20). Si no lo es, algo esta mal y no se toca la cola.
    intptr_t headIdx = ((intptr_t)head - (intptr_t)arr) / 0x14;
    if (headIdx < 0 || headIdx > 100000) { Log("  cola de carga: la cabeza %p no esta en el array (indice %d): no se mete en la cola", head, (int)headIdx); return false; }
    char* info = (char*)arr + (uintptr_t)modelId * 0x14;
    int16_t oldFirst = *(int16_t*)head;            // +0 del centinela: primer modelo (o -1 si vacia)
    if (oldFirst < -1 || oldFirst > 100000) { Log("  cola de carga: el primer modelo %d no es sano: no se mete en la cola", (int)oldFirst); return false; }
    // Si el modelo YA esta en la cola, no se vuelve a meter (3.1.17): meterlo
    // dos veces hace que su +0 apunte a si mismo (bucle en la lista) y el
    // juego se congela dentro de GetNextFileOnCd al recorrer la cola. Se
    // recorre la lista desde la cabeza siguiendo +0 hasta encontrarlo, o
    // hasta llegar al final (-1) o a la cabeza; con tope para no dar vueltas
    // si la cola ya estuviera corrupta.
    int16_t cur = oldFirst;
    for (int i = 0; i < 100000 && cur != -1 && (intptr_t)cur != headIdx; i++) {
        if (cur == modelId) return true;           // ya esta en la cola: no se toca
        if (cur < 0 || cur > 100000) break;        // indice raro: salirse
        cur = *(int16_t*)((char*)arr + (uintptr_t)(uint16_t)cur * 0x14 + 0);
    }
    *(int16_t*)(info + 0) = oldFirst;              // el modelo apunta al primero viejo
    *(int16_t*)(info + 2) = (int16_t)headIdx;      // el modelo apunta atras al centinela
    *(int16_t*)head = (int16_t)modelId;            // el centinela apunta al modelo
    if (oldFirst != -1)
        *(int16_t*)((char*)arr + (uintptr_t)(uint16_t)oldFirst * 0x14 + 2) = (int16_t)modelId;  // el primero viejo apunta atras al modelo
    info[0x10] = 2;                                // estado = en la cola
    if (g_reqCountGlobal) {
        int* cnt = *(int**)g_reqCountGlobal;
        if (cnt) *cnt = *cnt + 1;
    }
    return true;
}

// Registra un .txd propio en el CTxdStore y rellena su CStreamingInfo, como
// hace LoadCdDirectory para los .txd (0x557a04-0x557b78). name ya viene
// truncado en '.' ("suk"); imgPath es la ruta resuelta del IMG propio;
// rawOffset es el offset en sectores SIN el slot (el slot va aparte, en el
// byte +7 del CStreamingInfo); size es el tamano en sectores (con la regla
// del juego: si la mitad alta en +6 es distinta de 0, esa es el tamano).
// Los campos se escriben SIEMPRE (no solo si GetCdPosnAndSize dice que no
// estan): tras cargar una partida nueva el slot cambia y el valor viejo
// quedaria apuntando al slot anterior. Si el nombre ya estaba registrado
// (por ejemplo, en un re-registro), se reutiliza su hueco.
// Devuelve true si el .txd quedo registrado (o ya lo estaba con sus datos
// puestos); false si falta alguna funcion o no se localizo el array.
static bool RegisterTxd(const char* name, const char* imgPath, uint32_t rawOffset, uint32_t size, int slot) {
    if (!g_txdFindSlot || !g_txdAddSlot) {
        static bool said = false;
        if (!said) { said = true; Log("sin FindTxdSlot/AddTxdSlot: los .txd propios no se registraran (las skins se veran sin texturas)"); }
        return false;
    }
    int txdSlot = g_txdFindSlot(name);
    if (txdSlot < 0) {
        txdSlot = g_txdAddSlot(name, imgPath, false);
        if (txdSlot < 0) { Log("no se pudo registrar el .txd '%s' en el CTxdStore", name); return false; }
    }
    if (!g_infoSetCdPosnAndSize) {
        Log("sin SetCdPosnAndSize: el .txd '%s' quedo registrado por nombre pero sin sus datos (no se veran sus texturas)", name);
        return false;
    }
    void* arr = GetStreamingInfoArray();
    if (!arr) { Log("sin array CStreamingInfo: el .txd '%s' quedo registrado por nombre pero sin sus datos", name); return false; }
    int modelId = txdSlot + 0x4e20;   // el juego numera los modelos de .txd desde 0x4E20
    char* info = (char*)arr + (uintptr_t)modelId * 0x14;   // CStreamingInfo: 20 bytes por modelo
    info[7] = (char)slot;             // indice de imagen (el slot del IMG propio)
    info[6] = 0;                      // campo +6 a 0, como hace el juego
    g_infoSetCdPosnAndSize(info, rawOffset, size);   // +8 offset, +12 tamano
    *(int16_t*)(info + 4) = -1;      // +4 = fin de la cadena "siguiente en el CD" (3.1.14: si quedara en 0, el streaming seguiria hasta el modelo 0)
    Log("txd '%s' registrado (hueco %d, modelo %d, slot %d, offset %u, tamano %u sectores)", name, txdSlot, modelId, slot, rawOffset, size);
    // 3.1.19: el .txd ya NO se pide al streaming ni se mete en la cola. El
    // streaming dejaba el .txd en estado 1 para siempre (nunca terminaba de
    // cargar) y tocar la cola congelaba el juego. Ahora el .txd se carga
    // DIRECTO con RenderWare (LoadTxdDirect, desde los datos del IMG) en
    // cuanto se registra; eso lo hace LoadImgDirectory despues de llamar
    // aqui. El CStreamingInfo de arriba solo queda como registro de donde
    // esta el .txd dentro del IMG.
    return true;
}

// Pide el modelo del .txd de una skin al streaming (3.1.13). Se llama al
// aplicar la skin, justo despues de RequestSpecialChar y antes de
// LoadAllRequestedModels, para que el .txd se cargue en el mismo lote que el
// .dff. En 3.1.12 el .txd solo se pedia al registrar el IMG (precarga), pero
// la skin seguia en blanco: pedirlo tambien al aplicar cubre el caso de que
// la precarga no lo cargara o se descargara. Devuelve el modelo del .txd o
// -1 si no esta registrado o falta RequestModel.
// 3.1.18: ya NO se vuelve a meter en la cola aqui. En 3.1.16/3.1.17 meter el
// .txd en la cola al aplicar (cuando ya estaba en la cola desde el registro)
// congelaba el juego; el .txd solo se mete en la cola UNA vez, al registrar
// el IMG (RegisterTxd, como en 3.1.15, que no se cerraba ni se congelaba).
static int RequestTxd(const char* name) {
    if (!g_txdFindSlot || !g_requestModel) return -1;
    int txdSlot = g_txdFindSlot(name);
    if (txdSlot < 0) { Log("  txd de '%s': no esta registrado (FindTxdSlot = -1)", name); return -1; }
    int modelId = txdSlot + 0x4e20;
    g_requestModel(modelId, 0);
    Log("  txd de '%s' pedido al aplicar (modelo %d)", name, modelId);
    return modelId;
}

// Diagnostico (3.1.13): estado del modelo del .txd en su CStreamingInfo
// (byte +0x10: 0 = no pedido, 1 = pidiendo/cargando, 2 = cargado). Se llama
// cuando la skin ya se puso: si es 2, el .txd ESTA cargado y el problema es
// otro (las texturas del .txd no coinciden con las que pide el .dff); si es
// 0 o 1, el .txd no termino de cargar.
static void LogTxdStatus(const char* name) {
    if (!g_txdFindSlot) { Log("  txd de '%s': sin FindTxdSlot", name); return; }
    int txdSlot = g_txdFindSlot(name);
    if (txdSlot < 0) { Log("  txd de '%s': no registrado", name); return; }
    int modelId = txdSlot + 0x4e20;
    void* arr = GetStreamingInfoArray();
    if (!arr) { Log("  txd de '%s': sin array CStreamingInfo", name); return; }
    const uint8_t* info = (const uint8_t*)arr + (uintptr_t)modelId * 0x14;
    Log("  txd de '%s': hueco %d, modelo %d, estado = %d (0=no pedido, 1=pidiendo, 2=cargado), imagen %d, offset %u, tamano %u, diccionario %s",
        name, txdSlot, modelId, info[0x10], info[7], *(const uint32_t*)(info + 8), *(const uint32_t*)(info + 12),
        TxdDictLoaded(name) ? "CARGADO" : "sin cargar");
    // Volcado completo de los 20 bytes del CStreamingInfo (3.1.18): para ver
    // por que el .txd no termina de cargar (+0/+2 = enlaces de la cola,
    // +4 = siguiente en el CD, +6 = banderas, +7 = imagen, +8 = offset,
    // +12 = tamano, +0x10 = estado).
    Log("  txd de '%s': info = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        name, info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7], info[8], info[9], info[10], info[11], info[12], info[13], info[14], info[15], info[16], info[17], info[18], info[19]);
}

// Mete el directorio del IMG propio en el CDirectory del juego, entrada por
// entrada, con CDirectory::AddItem (sin pasar por LoadCdDirectory, que en
// partida se cae en GetEntry). Lee el IMG que ARCX mismo escribio (ruta
// absoluta, fopen de libc: ARCX lo escribio ahi). El offset de cada entrada
// lleva el indice del slot en el byte alto (como hace LoadCdDirectory); el
// tamano se lee como medio de 16 bits en +4, y ARCX escribe sectores de 32
// bits con la mitad alta en 0 (archivos pequenos), asi que se lee bien.
// IMPORTANTE (3.1.10): el nombre se trunca en el primer '.' antes de meterlo,
// exactamente como hace LoadCdDirectory (0x557bcc-0x557bf8): "suk.dff" se
// guarda como "suk", porque RequestSpecialModel busca con FindItem el nombre
// SIN extension. Los .dff van al CDirectory; los .txd NO van al CDirectory,
// se registran en el CTxdStore con RegisterTxd (3.1.11) para que la skin
// tenga sus texturas. Las entradas que ya estan (re-registro tras cargar
// partida) se saltan con FindItem, para no duplicarlas. AddItem se cae si el
// CDirectory esta lleno: se comprueba la capacidad antes (+8 capacidad,
// +12 conteo).
static bool LoadImgDirectory(int slot, void* cdDir) {
    if (!cdDir || !g_cdDirAddItem) {
        Log("sin CDirectory del juego: el IMG se registro pero su directorio no se cargo (las skins propias no cargaran)");
        return false;
    }
    FILE* f = fopen(g_imgPath.c_str(), "rb");
    if (!f) { Log("no se pudo reabrir el IMG propio %s para cargar su directorio", g_imgPath.c_str()); return false; }
    char magic[4]; uint32_t count = 0;
    bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "VER2", 4) == 0 && fread(&count, 4, 1, f) == 1 && count > 0 && count < 1000;
    if (!ok) { fclose(f); Log("el IMG propio no tiene cabecera VER2 valida: su directorio no se cargo"); return false; }
    int32_t cap = *(int32_t*)((char*)cdDir + 8);
    int32_t cur = *(int32_t*)((char*)cdDir + 12);
    if (cur + (int32_t)count > cap) { fclose(f); Log("el CDirectory del juego esta lleno (%d/%d): no caben las %u entradas del IMG propio", cur, cap, count); return false; }
    int added = 0, skipped = 0, txdOk = 0, txdFail = 0, other = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t ent[32];
        if (fread(ent, 1, 32, f) != 32) { ok = false; break; }
        ent[31] = '\0';   // el nombre va en +8 (24 bytes); se termina en +31 como hace el juego
        char* name = (char*)ent + 8;
        if (name[0] == '\0') continue;
        char* dot = strchr(name, '.');
        if (!dot) continue;   // sin extension: el juego lo ignora
        size_t namelen = (size_t)(dot - name);
        if (namelen == 0 || namelen >= 21) continue;   // el juego ignora nombres de 0 o >= 21 caracteres antes del '.'
        bool isDff = strncasecmp(dot + 1, "dff", 3) == 0 && dot[4] == '\0';
        bool isTxd = strncasecmp(dot + 1, "txd", 3) == 0 && dot[4] == '\0';
        if (!isDff && !isTxd) { other++; continue; }   // solo .dff y .txd se registran (el resto no va a ningun lado)
        *dot = '\0';   // "suk.dff" -> "suk", "suk.txd" -> "suk": asi lo guarda el juego y asi lo busca RequestSpecialModel
        if (isDff) {
            if (g_cdDirFindItem) {
                uint32_t off = 0, sz = 0;
                if (g_cdDirFindItem(cdDir, name, &off, &sz)) { skipped++; continue; }   // ya esta: no se duplica
            }
            uint32_t off = *(uint32_t*)ent;
            off |= ((uint32_t)slot << 24);
            *(uint32_t*)ent = off;
            g_cdDirAddItem(cdDir, ent);
            added++;
        } else {   // .txd: al CTxdStore, no al CDirectory
            uint32_t rawOff = *(uint32_t*)ent;   // offset SIN el slot (el slot va aparte, en el byte +7 del CStreamingInfo)
            uint16_t sizeHi = *(uint16_t*)(ent + 6);
            uint32_t size = sizeHi != 0 ? sizeHi : *(uint16_t*)(ent + 4);   // regla del juego: si la mitad alta es distinta de 0, esa es el tamano
            if (RegisterTxd(name, g_resolvedPath.c_str(), rawOff, size, slot)) {
                txdOk++;
                // Cargar el .txd DIRECTO (3.1.19): leer sus datos del IMG y
                // crear su diccionario de texturas con RenderWare, sin pasar
                // por la cola de streaming del juego. Se guarda la posicion
                // actual del archivo y se vuelve a ella para seguir leyendo
                // el directorio (el bucle usa las entradas en memoria, pero
                // el fread del directorio sigue despues).
                long dirPos = ftell(f);
                if (fseek(f, (long)rawOff * 2048, SEEK_SET) == 0) {
                    size_t bytes = (size_t)size * 2048;
                    uint8_t* buf = (uint8_t*)malloc(bytes);
                    if (buf && fread(buf, 1, bytes, f) == bytes) {
                        LoadTxdDirect(name, buf, bytes);   // guarda el buffer (no se libera)
                    } else {
                        Log("  no se pudo leer el .txd '%s' del IMG (offset %u, %u sectores)", name, rawOff, size);
                        if (buf) free(buf);
                    }
                } else {
                    Log("  no se pudo buscar el .txd '%s' en el IMG (offset %u)", name, rawOff);
                }
                fseek(f, dirPos, SEEK_SET);
            }
            else txdFail++;
        }
    }
    fclose(f);
    if (!ok) { Log("el IMG propio se leyo incompleto: su directorio quedo a medias"); return false; }
    Log("directorio del IMG cargado en el juego (slot %d): %d .dff nuevos, %d ya estaban, %d .txd registrados, %d .txd sin registrar, %d otros ignorados", slot, added, skipped, txdOk, txdFail, other);
    return true;
}

// Registra el IMG en el sistema de streaming del juego (como hace ZyZ con el
// suyo) y carga su directorio en el juego, para que FindItem encuentre los
// .dff/.txd. Solo AddImageToList NO basta (verificado desensamblando
// libGTASA.so 2.10): AddImageToList solo guarda la ruta y abre el archivo.
// El directorio se carga DIRECTO con CDirectory::AddItem (ver arriba): NO se
// llama a CStreaming::LoadCdDirectory en partida, porque en 3.1.7 se cerro
// el juego ahi dentro (GetEntry con su tabla a NULL fuera de la carga de
// nivel). Sin el directorio, en 3.1.4/3.1.5 el juego nunca encontraba
// 'suk.dff' y el pedido quedaba atascado para siempre (y atascaba tambien
// las skins de ZyZ que se pedian despues).
// Solo una vez por sesion; si alguna funcion no existe en libGTASA.so, las
// skins propias quedan como antes (se listan, no se aplican, sin crash).
// El slot se reserva UNA sola vez por sesion y se conserva entre reintentos:
// en 3.1.8, fallar el directorio y reintentarlo a cada frame llamaba a
// AddImageToList una y otra vez hasta llenar la lista de imagenes (36
// huecos) y agotar los manejadores de CdStreamOpen, y el juego se cerro en
// CdStreamRead. Tras un fallo se espera ~10 s (600 frames) antes de
// reintentarlo, sin volver a reservar slot ni repetir la prueba de ruta.
static bool RegisterImg(int (*addImageToList)(const char*, bool), uint64_t (*openFile)(const char*, const char*), void (*closeFile)(uint64_t), int frame) {
    if (g_imgRegistered.load()) return true;
    if (!g_imgBuilt.load() || !addImageToList) return false;
    if (!g_cdDirAddItem) {
        static bool said = false;
        if (!said) { said = true; Log("no se pudo localizar el CDirectory del juego: el IMG propio no se registrara (las de ZyZ si funcionan)"); }
        return false;
    }
    if (frame < g_nextDirTryFrame) return false;   // enfriamiento tras un fallo: no se reintenta a cada frame
    void* cdDir = GetCdDirectory();
    if (!cdDir) {
        Log("el CDirectory del juego aun no esta listo: se reintentara en ~10 s");
        g_nextDirTryFrame = frame + 600;
        return false;
    }
    if (g_imgSlot < 0) {
        std::string path = g_resolvedPath.empty() ? ResolveImgPath(openFile, closeFile) : g_resolvedPath;
        if (path.empty()) { g_nextDirTryFrame = frame + 600; return false; }   // ninguna ruta abre el IMG: no se registra (registrarlo atasca el canal)
        g_resolvedPath = path;
        int idx = addImageToList(path.c_str(), true);
        if (idx <= 0) {   // 0 = sin hueco libre en la lista de imagenes (la lista tiene 36 huecos por un mod; el slot 0 es gta3.img, nunca libre)
            Log("no hay hueco libre en la lista de imagenes del juego: el IMG propio no se registro (se reintentara en ~10 s)");
            g_nextDirTryFrame = frame + 600;
            return false;
        }
        g_imgSlot = idx;
    }
    if (!LoadImgDirectory(g_imgSlot, cdDir)) { g_nextDirTryFrame = frame + 600; return false; }   // el slot se conserva para el reintento
    g_imgRegistered = true;
    Log("IMG registrado en el juego: %s (slot %d, las skins propias ya se pueden pedir)", g_resolvedPath.c_str(), g_imgSlot);
    return true;
}
static bool ImgReady() { return g_imgBuilt.load(); }
static bool ImgRegistered() { return g_imgRegistered.load(); }

// ---- redireccion de archivos (fopen de libGTASA.so) -------------------------
static std::mutex g_redirMtx;
static std::map<std::string, std::string> g_redirects;   // nombre en minusculas -> ruta real
static FILE* (*g_fopenOrig)(const char*, const char*) = nullptr;
static std::atomic<int> g_redirLogs{0};

// Reconstruye la tabla con las skins propias actuales. Se llama al consumir
// cada escaneo (hilo del juego). Con el cargador apagado la tabla queda vacia.
static void Rebuild(const std::vector<skinscan::Skin>& skins) {
    std::map<std::string, std::string> t;
    for (auto& s : skins) {
        if (s.src != 1) continue;
        std::string low = skinscan::Lower(s.name);
        t[low + ".dff"] = s.path;
        if (s.hasTxd) {
            t[low + ".txd"] = s.txdPath;
            std::string tb = s.txdPath.substr(s.txdPath.find_last_of('/') + 1);
            t[skinscan::Lower(tb)] = s.txdPath;
        }
    }
    std::lock_guard<std::mutex> lk(g_redirMtx);
    g_redirects = std::move(t);
    Log("redirecciones activas: %zu archivo(s) propio(s)", g_redirects.size());
}

static std::string Lookup(const char* baseNameLower) {
    std::lock_guard<std::mutex> lk(g_redirMtx);
    auto it = g_redirects.find(baseNameLower);
    return it == g_redirects.end() ? std::string() : it->second;
}

static FILE* FopenHook(const char* path, const char* mode) {
    if (!path || !mode || !g_fopenOrig) return g_fopenOrig ? g_fopenOrig(path, mode) : nullptr;
    if (strchr(mode, 'r')) {
        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        std::string to = Lookup(skinscan::Lower(base).c_str());
        if (!to.empty()) {
            int n = g_redirLogs.fetch_add(1);
            if (n < 20) Log("fopen: '%s' -> '%s'", path, to.c_str());
            return g_fopenOrig(to.c_str(), mode);
        }
    }
    return g_fopenOrig(path, mode);
}

// ---- diagnostico: el juego puede pedir los archivos por open/openat --------
// En 3.1.2 el gancho de fopen se instalo bien y la tabla tenia 2 archivos, pero
// el juego NUNCA pidio 'suk.dff'/'suk.txd' por fopen (cero redirecciones en el
// log): sin registro previo del nombre, el juego ni siquiera llega a pedir el
// archivo. Estos ganchos comprueban si lo pide por open/openat en su lugar.
// El filtro por extension va ANTES de mirar la tabla, para no frenar al juego
// (open/openat se llaman muchisimo): sin .dff/.txd no se toca el mutex.
// Solo se desvian lecturas de archivos registrados; lo demas pasa tal cual.
static int (*g_openOrig)(const char*, int, ...) = nullptr;
static int (*g_openatOrig)(int, const char*, int, ...) = nullptr;
static std::atomic<int> g_openLogs{0};

static bool IsModelFile(const char* base) {
    size_t n = strlen(base);
    if (n < 4) return false;
    const char* ext = base + n - 4;
    auto eq = [](const char* a, const char* b) {
        for (int i = 0; i < 4; i++) {
            char ca = a[i], cb = b[i];
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) return false;
        }
        return true;
    };
    return eq(ext, ".dff") || eq(ext, ".txd");
}

static int OpenHook(const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
    if (path && (flags & O_ACCMODE) == O_RDONLY && g_openOrig) {
        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        if (IsModelFile(base)) {
            std::string to = Lookup(skinscan::Lower(base).c_str());
            if (!to.empty()) {
                int n = g_openLogs.fetch_add(1);
                if (n < 20) Log("open: '%s' -> '%s'", path, to.c_str());
                return g_openOrig(to.c_str(), flags, mode);
            }
        }
    }
    return g_openOrig ? g_openOrig(path, flags, mode) : -1;
}

static int OpenAtHook(int dirfd, const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
    if (path && (flags & O_ACCMODE) == O_RDONLY && g_openatOrig) {
        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        if (IsModelFile(base)) {
            std::string to = Lookup(skinscan::Lower(base).c_str());
            if (!to.empty()) {
                int n = g_openLogs.fetch_add(1);
                if (n < 20) Log("openat: '%s' -> '%s'", path, to.c_str());
                return g_openatOrig(dirfd, to.c_str(), flags, mode);
            }
        }
    }
    return g_openatOrig ? g_openatOrig(dirfd, path, flags, mode) : -1;
}

// Parchea la entrada GOT de fopen en libGTASA.so (mismo patron que el menu
// usa para eglSwapBuffers). Solo afecta a las llamadas que hace el juego.
static bool PatchGot(const char* libName, const char* sym, void* repl, void** orig) {
    struct Ctx { const char* lib; uintptr_t base; const ElfW(Phdr)* ph; int n; } c = {libName, 0, nullptr, 0};
    dl_iterate_phdr([](struct dl_phdr_info* i, size_t, void* d) -> int {
        Ctx* x = (Ctx*)d;
        if (i->dlpi_name && strstr(i->dlpi_name, x->lib)) {
            x->base = i->dlpi_addr; x->ph = i->dlpi_phdr; x->n = i->dlpi_phnum;
            return 1;
        }
        return 0;
    }, &c);
    if (!c.ph) { Log("GOT: no encuentro %s en memoria", libName); return false; }
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < c.n; i++)
        if (c.ph[i].p_type == PT_DYNAMIC) dyn = (const ElfW(Dyn)*)(c.base + c.ph[i].p_vaddr);
    if (!dyn) return false;
    auto fix = [&](uintptr_t p) { return p < c.base ? p + c.base : p; };
    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;
    const ElfW(Rela)* jmprel = nullptr; size_t jmpsz = 0;
    const ElfW(Rela)* rela = nullptr;   size_t relasz = 0;
    for (; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
            case DT_SYMTAB:   symtab = (const ElfW(Sym)*)fix(dyn->d_un.d_ptr); break;
            case DT_STRTAB:   strtab = (const char*)fix(dyn->d_un.d_ptr); break;
            case DT_JMPREL:   jmprel = (const ElfW(Rela)*)fix(dyn->d_un.d_ptr); break;
            case DT_PLTRELSZ: jmpsz = dyn->d_un.d_val; break;
            case DT_RELA:     rela = (const ElfW(Rela)*)fix(dyn->d_un.d_ptr); break;
            case DT_RELASZ:   relasz = dyn->d_un.d_val; break;
            default: break;
        }
    }
    if (!symtab || !strtab) return false;
    auto scan = [&](const ElfW(Rela)* r, size_t bytes) -> bool {
        if (!r) return false;
        for (size_t k = 0; k < bytes / sizeof(ElfW(Rela)); k++) {
            unsigned type = ELF64_R_TYPE(r[k].r_info);
            unsigned si = ELF64_R_SYM(r[k].r_info);
            if ((type != 1026 && type != 1025) || si == 0) continue;   // JUMP_SLOT / GLOB_DAT (aarch64)
            if (strcmp(strtab + symtab[si].st_name, sym) != 0) continue;
            void** slot = (void**)(c.base + r[k].r_offset);
            long pg = sysconf(_SC_PAGESIZE);
            uintptr_t page = (uintptr_t)slot & ~((uintptr_t)pg - 1);
            if (mprotect((void*)page, (size_t)pg, PROT_READ | PROT_WRITE) != 0) return false;
            *orig = *slot;
            *slot = repl;
            return true;
        }
        return false;
    };
    return scan(jmprel, jmpsz) || scan(rela, relasz);
}

static bool g_hookInstalled = false;
static bool InstallHook() {
    if (g_hookInstalled) return true;
    if (!PatchGot("libGTASA.so", "fopen", (void*)FopenHook, (void**)&g_fopenOrig)) {
        Log("fopen no encontrado en la GOT de libGTASA.so: el cargador propio no podra servir archivos (las skins de ARCXLoader no cargaran)");
        return false;
    }
    g_hookInstalled = true;
    Log("fopen redirigido: las skins de ARCXLoader se sirven desde su carpeta");
    // Diagnostico: si el juego pide los archivos por open/openat en vez de
    // fopen, se redirigen igual (solo .dff/.txd registrados, solo lectura).
    // Si el log no muestra ninguna de las tres, el juego nunca pide el archivo.
    if (PatchGot("libGTASA.so", "open", (void*)OpenHook, (void**)&g_openOrig))
        Log("open redirigido (diagnostico)");
    if (PatchGot("libGTASA.so", "openat", (void*)OpenAtHook, (void**)&g_openatOrig))
        Log("openat redirigido (diagnostico)");
    return true;
}

}  // namespace loader
