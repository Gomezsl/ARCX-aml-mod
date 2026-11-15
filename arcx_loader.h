// arcx_loader.h - cargador propio de skins de Project ARCX (carpeta ARCXLoader/).
// No depende de ZyZ ModLoader: cada skin vive en su propia subcarpeta,
//   ARCXLoader/<Nombre>/skin.dff (+ su .txd y texturas .png/.jpg sueltas)
// El nombre del modelo es el del .dff sin extension (max. 23 caracteres, como
// en modloader/). Si dos carpetas traen un .dff con el mismo nombre, solo
// entra la primera y la otra queda registrada en el log como duplicada.
//
// COMO CARGA EL JUEGO LOS MODELOS (probado en 3.1.2 y 3.1.3): el juego NO abre
// los .dff/.txd sueltos por fopen/open/openat (cero redirecciones en los logs
// aunque los ganchos estaban instalados): los carga desde archivos .img
// registrados en su sistema de streaming. ZyZ hace exactamente eso: construye
// su ZYZML\GTA3.IMG y lo registra con CStreaming::AddImageToList (su .so lo
// confirma: "_ZN10CStreaming14AddImageToListEPKcb"). El cargador propio hace
// lo mismo: construye arcxml/gta3.img dentro de ARCXLoader/ con los .dff/.txd
// de las skins propias y lo registra en el juego, asi RequestSpecialChar
// encuentra el archivo sin depender de ZyZ. El gancho de fopen/open/openat
// queda solo como diagnostico (por si el juego pidiera algun archivo suelto).
// Las texturas .png/.jpg se inventarian y se muestran en el log, pero NO se
// convierten a texturas: para que la skin se vea con texturas necesita su
// .txd (esa conversion la hacia ZyZ; aqui no se reimplementa).
#pragma once
#include <cstdio>
#include <cstdarg>
#include <cstring>
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
    g_lastSig = sig;
    g_imgPath = imgPath;
    g_imgBuilt = true;
    Log("IMG de ARCXLoader construido: %s (%u archivo(s))", imgPath.c_str(), count);
    return true;
}

// Registra el IMG en el sistema de streaming del juego (como hace ZyZ con el
// suyo). Solo una vez por sesion; si la funcion no existe en libGTASA.so, las
// skins propias quedan como antes (se listan, no se aplican, sin crash).
static bool RegisterImg(void (*addImageToList)(const char*, bool)) {
    if (g_imgRegistered.load()) return true;
    if (!g_imgBuilt.load() || !addImageToList) return false;
    addImageToList(g_imgPath.c_str(), true);
    g_imgRegistered = true;
    Log("IMG registrado en el juego: %s (las skins propias ya se pueden pedir)", g_imgPath.c_str());
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
