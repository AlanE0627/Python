// maple-offline: minimal Microsoft Cabinet (MSZIP) extractor for the GMS v83 installer.
// The InstallShield self-extracting installer embeds a 2-volume cabinet set
// (MapleStory_1.cab + MapleStory_2.cab) directly after the PE stub. Volumes are found
// by their "MSCF" signature; folders and data blocks spanning the volume boundary
// are joined. Only MSZIP compression is supported (the only type used by this installer).
#include "setup.h"

#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace msetup {
namespace {

struct Mapped {
    const uint8_t *p = nullptr;
    size_t n = 0;
    int fd = -1;
    bool open(const std::string &path) {
        fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;
        struct stat st {};
        if (fstat(fd, &st) != 0) return false;
        n = (size_t) st.st_size;
        void *m = mmap(nullptr, n, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m == MAP_FAILED) return false;
        p = (const uint8_t *) m;
        return true;
    }
    ~Mapped() {
        if (p) munmap((void *) p, n);
        if (fd >= 0) ::close(fd);
    }
};

template <class T> T rd(const uint8_t *p) { T v; std::memcpy(&v, p, sizeof(T)); return v; }

struct Block { const uint8_t *data; uint32_t len; uint16_t uncomp; };
struct CabFolder { uint32_t coffStart; uint16_t nData; uint16_t type; };
struct CabFile { std::string name; uint32_t size; uint32_t off; uint16_t folder; };
struct Cab {
    const uint8_t *base; uint32_t size; uint16_t flags; uint16_t index;
    uint8_t resFolder = 0, resData = 0;
    std::vector<CabFolder> folders;
    std::vector<CabFile> files;
};

bool parse_cab(const uint8_t *b, size_t avail, Cab &c, std::string &err) {
    if (avail < 36 || std::memcmp(b, "MSCF", 4) != 0) { err = "bad cab signature"; return false; }
    c.base = b;
    c.size = rd<uint32_t>(b + 8);
    uint32_t coffFiles = rd<uint32_t>(b + 16);
    uint16_t nFolders = rd<uint16_t>(b + 26);
    uint16_t nFiles = rd<uint16_t>(b + 28);
    c.flags = rd<uint16_t>(b + 30);
    c.index = rd<uint16_t>(b + 34);
    if (c.size > avail) { err = "cab truncated"; return false; }
    const uint8_t *q = b + 36;
    if (c.flags & 4) {
        uint16_t resHeader = rd<uint16_t>(q);
        c.resFolder = q[2];
        c.resData = q[3];
        q += 4 + resHeader;
    }
    auto skipz = [&]() { while (*q) q++; q++; };
    if (c.flags & 1) { skipz(); skipz(); }
    if (c.flags & 2) { skipz(); skipz(); }
    for (int i = 0; i < nFolders; i++) {
        c.folders.push_back({rd<uint32_t>(q), rd<uint16_t>(q + 4), rd<uint16_t>(q + 6)});
        q += 8 + c.resFolder;
    }
    q = b + coffFiles;
    for (int i = 0; i < nFiles; i++) {
        CabFile f;
        f.size = rd<uint32_t>(q);
        f.off = rd<uint32_t>(q + 4);
        f.folder = rd<uint16_t>(q + 8);
        q += 16;
        f.name = (const char *) q;
        q += f.name.size() + 1;
        for (auto &ch : f.name) if (ch == '\\') ch = '/';
        c.files.push_back(f);
    }
    for (auto &fo : c.folders) {
        if ((fo.type & 0xF) != 1) { err = "unsupported cab compression"; return false; }
    }
    return true;
}

std::vector<Block> folder_blocks(const Cab &c, const CabFolder &fo) {
    std::vector<Block> v;
    const uint8_t *q = c.base + fo.coffStart;
    for (int i = 0; i < fo.nData; i++) {
        uint16_t cb = rd<uint16_t>(q + 4);
        uint16_t un = rd<uint16_t>(q + 6);
        const uint8_t *d = q + 8 + c.resData;
        v.push_back({d, cb, un});
        q = d + cb;
    }
    return v;
}

}  // namespace

bool extract_installer(const std::string &setupExe, const std::string &outDir,
                       const Progress &progress, std::string &err) {
    Mapped m;
    if (!m.open(setupExe)) { err = "cannot open installer"; return false; }

    // Locate cabinet volumes by signature (reserved field after "MSCF" is 0).
    std::vector<Cab> cabs;
    static const uint8_t sig[8] = {'M', 'S', 'C', 'F', 0, 0, 0, 0};
    for (size_t i = 0; i + 36 < m.n;) {
        const void *hit = memmem(m.p + i, m.n - i, sig, sizeof(sig));
        if (!hit) break;
        size_t at = (const uint8_t *) hit - m.p;
        uint32_t size = rd<uint32_t>(m.p + at + 8);
        uint16_t major = m.p[at + 25];
        if (size > 1000000 && major == 1 && at + size <= m.n) {
            Cab c;
            if (!parse_cab(m.p + at, m.n - at, c, err)) return false;
            cabs.push_back(std::move(c));
            i = at + size;
        } else {
            i = at + 4;
        }
    }
    if (cabs.empty()) { err = "no cabinet found in installer"; return false; }

    // Build logical folders across volumes, joining a split last/first data block.
    std::vector<std::vector<Block>> logical;
    std::vector<std::vector<uint8_t>> joined;  // owns joined split blocks
    joined.reserve(cabs.size());
    std::vector<std::vector<int>> folderMap(cabs.size());
    for (size_t ci = 0; ci < cabs.size(); ci++) {
        const Cab &c = cabs[ci];
        for (size_t fi = 0; fi < c.folders.size(); fi++) {
            std::vector<Block> blocks = folder_blocks(c, c.folders[fi]);
            bool continuesPrev = ci > 0 && fi == 0 && (c.flags & 1);
            if (continuesPrev && !logical.empty()) {
                auto &prev = logical.back();
                if (!prev.empty() && prev.back().uncomp == 0 && !blocks.empty()) {
                    joined.emplace_back();
                    auto &j = joined.back();
                    j.insert(j.end(), prev.back().data, prev.back().data + prev.back().len);
                    j.insert(j.end(), blocks[0].data, blocks[0].data + blocks[0].len);
                    prev.back() = {j.data(), (uint32_t) j.size(), blocks[0].uncomp};
                    blocks.erase(blocks.begin());
                }
                prev.insert(prev.end(), blocks.begin(), blocks.end());
                folderMap[ci].push_back((int) logical.size() - 1);
            } else {
                logical.push_back(std::move(blocks));
                folderMap[ci].push_back((int) logical.size() - 1);
            }
        }
    }

    // Collect unique files per logical folder.
    struct Out { std::string name; uint32_t size; uint32_t off; };
    std::map<int, std::vector<Out>> perFolder;
    std::map<std::string, bool> seen;
    uint64_t total = 0;
    for (size_t ci = 0; ci < cabs.size(); ci++) {
        for (auto &f : cabs[ci].files) {
            int lf;
            if (f.folder == 0xFFFD || f.folder == 0xFFFF) lf = folderMap[ci].front();
            else if (f.folder == 0xFFFE) lf = folderMap[ci].back();
            else lf = folderMap[ci][f.folder];
            if (seen[f.name]) continue;
            seen[f.name] = true;
            perFolder[lf].push_back({f.name, f.size, f.off});
            total += f.size;
        }
    }

    uint64_t done = 0;
    std::vector<uint8_t> window;  // last 32 KiB of output for MSZIP dictionary
    std::vector<uint8_t> out(32768 + 1024);
    for (auto &[lf, files] : perFolder) {
        std::sort(files.begin(), files.end(), [](const Out &a, const Out &b) { return a.off < b.off; });
        size_t fileIdx = 0;
        FILE *cur = nullptr;
        uint64_t pos = 0;  // uncompressed position in folder
        window.clear();
        auto open_next = [&]() {
            if (fileIdx < files.size()) {
                std::string path = outDir + "/" + files[fileIdx].name;
                for (size_t k = 1; k < path.size(); k++)
                    if (path[k] == '/') { path[k] = 0; mkdir(path.c_str(), 0755); path[k] = '/'; }
                cur = std::fopen(path.c_str(), "wb");
            }
        };
        for (const Block &b : logical[lf]) {
            if (b.len < 2 || b.data[0] != 'C' || b.data[1] != 'K') { err = "bad MSZIP block"; return false; }
            z_stream zs {};
            inflateInit2(&zs, -15);
            if (!window.empty()) inflateSetDictionary(&zs, window.data(), (uInt) window.size());
            zs.next_in = (Bytef *) b.data + 2;
            zs.avail_in = b.len - 2;
            zs.next_out = out.data();
            zs.avail_out = (uInt) out.size();
            int r = inflate(&zs, Z_FINISH);
            size_t got = out.size() - zs.avail_out;
            inflateEnd(&zs);
            if (r != Z_STREAM_END || got != b.uncomp) { err = "MSZIP inflate failed"; return false; }
            window.assign(out.begin(), out.begin() + got);

            // Route the decompressed bytes to the files covering [pos, pos+got).
            size_t used = 0;
            while (used < got && fileIdx < files.size()) {
                const Out &f = files[fileIdx];
                uint64_t fStart = f.off, fEnd = (uint64_t) f.off + f.size;
                uint64_t here = pos + used;
                if (here < fStart) { used += (size_t) std::min<uint64_t>(fStart - here, got - used); continue; }
                if (!cur) open_next();
                if (!cur) { err = "cannot create " + f.name; return false; }
                size_t n = (size_t) std::min<uint64_t>(fEnd - here, got - used);
                std::fwrite(out.data() + used, 1, n, cur);
                used += n;
                done += n;
                if (here + n == fEnd) { std::fclose(cur); cur = nullptr; fileIdx++; }
            }
            pos += got;
            if (progress) progress("extract", done, total);
            if (fileIdx >= files.size()) break;
        }
        if (cur) std::fclose(cur);
        if (fileIdx < files.size()) { err = "cabinet data ended early"; return false; }
    }
    return true;
}

}  // namespace msetup
