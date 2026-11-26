// upkg publishing: one .ipk of a built release, added to the upkg third-party feed.
//
// UP-Term owns the spec (vtcon docs/upkg-publish.md) and the reference tools (vtcon tools/pkgs.py build-ipk and
// feed-index). Desktop users have no vtcon checkout, so this is our own builder, held byte-equal to pkgs.py by the
// fixtures in testdata/upkg (written by pkgs.py at vtcon 19ae3a01): an ar archive of debian-binary "2.0",
// control.tar.gz and data.tar.gz, GNU tar entries (mtime 0, root/root, os.walk order: a drawer's files before its
// sub-drawers, the whole tar padded to 10240 bytes), gzip level 9 with mtime 0 (zlib, as Python's gzip).
// The stage tree's top drawers are AmigaDOS volumes or assigns and get no tar entry: upkg's dest root is / and
// ixemul reads /Work/x as Work:x, so ./Work/Utilities/<BASE>/... installs into Work:Utilities/<BASE>.
// The feed is the release `feed` of spotUP/upkg-feed (never the UP-Term feed spotUP/vtcon), written with the gh
// CLI, so the publisher's `gh auth login` or GH_TOKEN is the only credential. No signing (upkg checks none).
use flate2::{Compression, GzBuilder};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

/// The architecture of a Release Dropper package (68020+; every installed upkg.conf lists it).
pub const ARCH: &str = "m68k-amigaos";
/// The third-party feed: GitHub repository and its one release tag (marked latest).
pub const FEED_REPO: &str = "spotUP/upkg-feed";
pub const FEED_TAG: &str = "feed";
const ARCHES: [&str; 3] = ["m68k-amigaos", "m68k-amigaos-fpu", "all"];
/// AmigaDOS file name limit: the .ipk name and every path component of the stage tree.
const NAME_MAX: usize = 30;
/// opkg keeps <Package>.conffiles/.control/.list in UP-Term:pkg/upkg/info, so 20 + ".conffiles" fits in 30.
const PACKAGE_MAX: usize = 20;
const CONTROL_FIELDS: [&str; 11] =
    ["Package", "Version", "Depends", "Replaces", "Conflicts", "Provides", "Launch", "Architecture", "Maintainer", "Installed-Size", "Description"];
const CONTROL_REQUIRED: [&str; 5] = ["Package", "Version", "Architecture", "Maintainer", "Description"];

#[derive(Debug, Clone, PartialEq)]
pub struct Manifest {
    pub package: String,
    pub version: String,
    pub revision: u32,
    pub description: String,
    pub maintainer: String,
    /// AmigaDOS drawer the release folder is installed into, e.g. "Work:Utilities".
    pub install_drawer: String,
}

impl Manifest {
    pub fn full_version(&self) -> String {
        format!("{}-r{}", self.version, self.revision)
    }
    /// <Package>_<upstream>_<Architecture>.ipk: no -r<rev>, a new revision replaces the asset of the same name.
    pub fn ipk_name(&self) -> String {
        format!("{}_{}_{}.ipk", self.package, self.version, ARCH)
    }
}

fn text(v: &Value, k: &str) -> String {
    v[k].as_str().unwrap_or("").trim().to_string()
}

fn package_ok(p: &str) -> bool {
    p.len() <= PACKAGE_MAX
        && !p.starts_with("up-")
        && p.starts_with(|c: char| c.is_ascii_lowercase() || c.is_ascii_digit())
        && p.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || "+-.".contains(c))
}

/// No '-' (it separates the revision) and no '_' (it separates the file name fields).
fn upstream_ok(v: &str) -> bool {
    v.starts_with(|c: char| c.is_ascii_digit()) && v.chars().all(|c| c.is_ascii_alphanumeric() || ".+~".contains(c))
}

/// The manifest of a publish request, or every problem with it.
pub fn manifest_from_request(r: &Value) -> Result<Manifest, Vec<String>> {
    let m = Manifest {
        package: text(r, "package"),
        version: text(r, "version"),
        revision: r["revision"].as_u64().unwrap_or(1) as u32,
        description: text(r, "description"),
        maintainer: text(r, "maintainer"),
        install_drawer: text(r, "installDrawer"),
    };
    let mut p = Vec::new();
    let package = package_ok(&m.package);
    if !package {
        p.push("the package name must be at most 20 lower case letters, digits, + - or ., start with a letter or digit, and not start with up- (reserved for the UP-Term feed)".into());
    }
    let version = upstream_ok(&m.version);
    if !version {
        p.push("the version must start with a digit and hold only letters, digits, . + or ~".into());
    }
    if package && version && m.ipk_name().chars().count() > NAME_MAX {
        let n = m.ipk_name();
        p.push(format!("the package file {n} is {} characters, the Amiga limit is 30 (shorten the package name or the version)", n.chars().count()));
    }
    if m.revision < 1 {
        p.push("the revision must be a whole number from 1".into());
    }
    if m.description.is_empty() || m.description.contains('\n') {
        p.push("the description must be one line".into());
    }
    if m.maintainer.is_empty() || m.maintainer.contains('\n') {
        p.push("the maintainer is missing".into());
    }
    if data_prefix(&m.install_drawer).is_none() {
        p.push("the install drawer must be an AmigaDOS path such as Work:Utilities, each name at most 30 characters".into());
    }
    if p.is_empty() {
        Ok(m)
    } else {
        Err(p)
    }
}

/// "Work:Utilities" -> ["Work", "Utilities"]; None for anything that is not a plain volume:drawer path.
fn data_prefix(drawer: &str) -> Option<Vec<String>> {
    let (vol, rest) = drawer.split_once(':')?;
    let ok = |s: &str| !s.is_empty() && s != "." && s != ".." && s.chars().count() <= NAME_MAX && !s.contains(['/', ':', '\\', '\n']);
    let mut parts = vec![vol.to_string()];
    parts.extend(rest.split('/').filter(|s| !s.is_empty()).map(str::to_string));
    parts.iter().all(|s| ok(s)).then_some(parts)
}

pub fn control_text(m: &Manifest, installed_size: u64) -> String {
    format!(
        "Package: {}\nVersion: {}\nArchitecture: {ARCH}\nMaintainer: {}\nInstalled-Size: {}\nDescription: {}\n",
        m.package,
        m.full_version(),
        m.maintainer,
        installed_size.div_ceil(1024),
        m.description
    )
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

// ---- reading an .ipk: its control, as pkgs.py ext_control checks it ----

fn split_ar(b: &[u8]) -> Result<Vec<(String, Vec<u8>)>, String> {
    if b.len() < 8 || &b[..8] != b"!<arch>\n" {
        return Err("not an ar archive".into());
    }
    let (mut i, mut out) = (8, Vec::new());
    while i + 60 <= b.len() {
        let h = std::str::from_utf8(&b[i..i + 60]).map_err(|_| "bad ar header")?;
        let size: usize = h[48..58].trim().parse().map_err(|_| "bad ar member size")?;
        let data = b.get(i + 60..i + 60 + size).ok_or("truncated ar member")?;
        out.push((h[..16].trim().trim_end_matches('/').to_string(), data.to_vec()));
        i += 60 + size + size % 2;
    }
    Ok(out)
}

/// (name, bytes) of every entry of a .tar.gz (plain ustar/GNU headers; long names are not joined).
fn tar_entries(gz: &[u8]) -> Result<Vec<(String, Vec<u8>)>, String> {
    let mut raw = Vec::new();
    flate2::read::GzDecoder::new(gz).read_to_end(&mut raw).map_err(|e| e.to_string())?;
    let cstr = |s: &[u8]| String::from_utf8_lossy(&s[..s.iter().position(|&c| c == 0).unwrap_or(s.len())]).into_owned();
    let (mut i, mut out) = (0, Vec::new());
    while i + 512 <= raw.len() && raw[i] != 0 {
        let h = &raw[i..i + 512];
        let size = usize::from_str_radix(cstr(&h[124..136]).trim(), 8).map_err(|_| "bad tar size")?;
        let data = raw.get(i + 512..i + 512 + size).ok_or("truncated tar entry")?;
        out.push((cstr(&h[..100]), data.to_vec()));
        i += 512 + size.div_ceil(512) * 512;
    }
    Ok(out)
}

fn ipk_control(ipk: &[u8]) -> Result<String, String> {
    let ctl = split_ar(ipk)?.into_iter().find(|(n, _)| n == "control.tar.gz").ok_or("no control.tar.gz")?.1;
    let c = tar_entries(&ctl)?.into_iter().find(|(n, _)| n == "./control").ok_or("no ./control")?.1;
    String::from_utf8(c).map_err(|_| "the control is not UTF-8".into())
}

type Control = Vec<(String, String)>;

fn field<'a>(c: &'a Control, k: &str) -> &'a str {
    c.iter().find(|(n, _)| n == k).map_or("", |(_, v)| v.as_str())
}

/// The fields of a third-party control, in file order, after the spec's checks.
fn parse_control(t: &str) -> Result<Control, String> {
    let mut c = Control::new();
    for line in t.lines().filter(|l| !l.trim().is_empty()) {
        let Some((k, v)) = line.split_once(": ").filter(|_| !line.starts_with([' ', '\t'])) else {
            return Err(format!("want one \"Key: value\" a line, got {line:?}"));
        };
        if !CONTROL_FIELDS.contains(&k) {
            return Err(format!("unknown field {k}"));
        }
        c.push((k.to_string(), v.trim().to_string()));
    }
    if let Some(k) = CONTROL_REQUIRED.iter().find(|k| field(&c, k).is_empty()) {
        return Err(format!("missing {k}"));
    }
    if !package_ok(field(&c, "Package")) {
        return Err(format!("Package {:?}: want [a-z0-9][a-z0-9+.-]*, at most 20 characters, no up- prefix", field(&c, "Package")));
    }
    let rev_ok = |r: &str| !r.is_empty() && !r.starts_with('0') && r.chars().all(|c| c.is_ascii_digit());
    if !field(&c, "Version").rsplit_once("-r").is_some_and(|(u, r)| upstream_ok(u) && rev_ok(r)) {
        return Err(format!("Version {:?}: want <upstream>-r<revision>, e.g. 1.2-r1", field(&c, "Version")));
    }
    if !ARCHES.contains(&field(&c, "Architecture")) {
        return Err(format!("Architecture {:?}: want one of {}", field(&c, "Architecture"), ARCHES.join(", ")));
    }
    Ok(c)
}

fn control_ipk_name(c: &Control) -> String {
    let upstream = field(c, "Version").rsplit_once("-r").map_or("", |(u, _)| u);
    format!("{}_{upstream}_{}.ipk", field(c, "Package"), field(c, "Architecture"))
}

/// The Packages text of every <dir>/*.ipk, as `pkgs.py feed-index` writes it: one stanza per file sorted by file
/// name, control fields in file order, then Filename, Size, SHA256sum and Description last.
pub fn feed_index(dir: &Path) -> Result<String, String> {
    let mut names: Vec<String> = fs::read_dir(dir)
        .map_err(|e| format!("{}: {e}", dir.display()))?
        .filter_map(|e| e.ok())
        .map(|e| e.file_name().to_string_lossy().into_owned())
        .filter(|n| n.ends_with(".ipk") && !n.starts_with('.'))
        .collect();
    names.sort();
    let mut blocks = Vec::new();
    for n in &names {
        let data = fs::read(dir.join(n)).map_err(|e| format!("{n}: {e}"))?;
        let c = ipk_control(&data).and_then(|t| parse_control(&t)).map_err(|e| format!("{n}: {e}"))?;
        let want = control_ipk_name(&c);
        if want != *n || n.chars().count() > NAME_MAX {
            return Err(format!("{n}: its control names it {want} (at most 30 characters)"));
        }
        let mut b: Vec<String> = c.iter().filter(|(k, _)| k != "Description").map(|(k, v)| format!("{k}: {v}")).collect();
        b.push(format!("Filename: {n}"));
        b.push(format!("Size: {}", data.len()));
        b.push(format!("SHA256sum: {}", hex(&Sha256::digest(&data))));
        b.push(format!("Description: {}", field(&c, "Description")));
        blocks.push(b.join("\n") + "\n");
    }
    if blocks.is_empty() {
        return Err(format!("no .ipk in {}", dir.display()));
    }
    Ok(blocks.join("\n"))
}

// ---- deterministic tar.gz and ar, as pkgs.py writes them with Python's tarfile GNU_FORMAT ----

enum Entry {
    Dir(String),
    File(String, Vec<u8>, u32),
}

fn octal(field: &mut [u8], v: u64) {
    let s = format!("{:0w$o}", v, w = field.len() - 1);
    field[..s.len()].copy_from_slice(s.as_bytes());
}

fn header(name: &[u8], size: u64, mode: u32, kind: u8, owner: &[u8]) -> [u8; 512] {
    let mut h = [0u8; 512];
    let n = name.len().min(100);
    h[..n].copy_from_slice(&name[..n]);
    octal(&mut h[100..108], mode as u64);
    octal(&mut h[108..116], 0);
    octal(&mut h[116..124], 0);
    octal(&mut h[124..136], size);
    octal(&mut h[136..148], 0);
    h[156] = kind;
    h[257..265].copy_from_slice(b"ustar  \0");
    h[265..265 + owner.len()].copy_from_slice(owner);
    h[297..297 + owner.len()].copy_from_slice(owner);
    h[148..156].copy_from_slice(b"        ");
    let sum: u32 = h.iter().map(|&b| b as u32).sum();
    h[148..155].copy_from_slice(format!("{sum:06o}\0").as_bytes());
    h
}

fn pad512(raw: &mut Vec<u8>) {
    raw.resize(raw.len().div_ceil(512) * 512, 0);
}

/// One entry's header blocks: a name over 100 bytes goes first in a GNU ././@LongLink entry, as tarfile does.
fn tar_header(raw: &mut Vec<u8>, name: &str, size: u64, mode: u32, kind: u8) {
    if name.len() > 100 {
        let mut long = name.as_bytes().to_vec();
        long.push(0);
        raw.extend(header(b"././@LongLink", long.len() as u64, 0, b'L', b""));
        raw.extend(long);
        pad512(raw);
    }
    raw.extend(header(name.as_bytes(), size, mode, kind, b"root"));
}

fn gzip(b: &[u8]) -> Result<Vec<u8>, String> {
    let mut gz = GzBuilder::new().mtime(0).write(Vec::new(), Compression::best());
    gz.write_all(b).map_err(|e| e.to_string())?;
    gz.finish().map_err(|e| e.to_string())
}

fn targz(entries: &[Entry]) -> Result<Vec<u8>, String> {
    let mut raw = Vec::new();
    for e in entries {
        match e {
            Entry::Dir(n) => tar_header(&mut raw, n, 0, 0o755, b'5'),
            Entry::File(n, data, mode) => {
                tar_header(&mut raw, n, data.len() as u64, *mode, b'0');
                raw.extend(data);
                pad512(&mut raw);
            }
        }
    }
    // two zero blocks, then the record padding (20 blocks) tarfile writes on close
    raw.extend([0u8; 1024]);
    raw.resize(raw.len().div_ceil(10240) * 10240, 0);
    gzip(&raw)
}

fn ar(members: &[(&str, &[u8])]) -> Vec<u8> {
    let mut out = b"!<arch>\n".to_vec();
    for (name, data) in members {
        out.extend(format!("{:<16}{:<12}{:<6}{:<6}{:<8}{:<10}`\n", format!("{name}/"), 0, 0, 0, "100644", data.len()).as_bytes());
        out.extend(*data);
        if data.len() % 2 == 1 {
            out.push(b'\n');
        }
    }
    out
}

/// Entries below `dir` in os.walk order (a drawer's files sorted, then each sub-drawer sorted, recursively).
fn walk(dir: &Path, top: &str, out: &mut Vec<Entry>, size: &mut u64) -> Result<(), String> {
    let (mut files, mut dirs) = (Vec::new(), Vec::new());
    for e in fs::read_dir(dir).map_err(|e| format!("{}: {e}", dir.display()))? {
        let e = e.map_err(|e| e.to_string())?;
        let name = e.file_name().to_string_lossy().into_owned();
        if name.chars().count() > NAME_MAX {
            return Err(format!("{} is over 30 characters, the Amiga limit", e.path().display()));
        }
        let t = e.file_type().map_err(|e| e.to_string())?;
        // shortcut: links are refused (pkgs.py packs them as tar symlinks); add that if a release ever ships one
        if t.is_symlink() {
            return Err(format!("{} is a symbolic link, which the package does not carry", e.path().display()));
        }
        if t.is_dir() { dirs.push(name) } else { files.push(name) }
    }
    files.sort();
    dirs.sort();
    for f in files {
        let p = dir.join(&f);
        let data = fs::read(&p).map_err(|e| format!("{}: {e}", p.display()))?;
        *size += data.len() as u64;
        #[cfg(unix)]
        let mode = if std::os::unix::fs::PermissionsExt::mode(&fs::metadata(&p).map_err(|e| e.to_string())?.permissions()) & 0o111 != 0 { 0o755 } else { 0o644 };
        #[cfg(not(unix))]
        let mode = 0o644;
        out.push(Entry::File(format!("{top}/{f}"), data, mode));
    }
    for d in dirs {
        let name = format!("{top}/{d}");
        out.push(Entry::Dir(format!("{name}/")));
        walk(&dir.join(&d), &name, out, size)?;
    }
    Ok(())
}

/// The .ipk bytes: the files of `tree` staged under <install drawer>/<folder>/.
pub fn build_ipk(m: &Manifest, tree: &Path, folder: &str) -> Result<Vec<u8>, String> {
    let parts = data_prefix(&m.install_drawer).ok_or("bad install drawer")?;
    if folder.chars().count() > NAME_MAX {
        return Err(format!("the release folder {folder} is over 30 characters, the Amiga limit"));
    }
    // From the first drawer down, never the volume itself: an entry for /Work makes opkg try to create it.
    let mut data = vec![Entry::Dir("./".into())];
    for i in 2..=parts.len() {
        data.push(Entry::Dir(format!("./{}/", parts[..i].join("/"))));
    }
    let top = format!("./{}/{folder}", parts.join("/"));
    data.push(Entry::Dir(format!("{top}/")));
    let mut size = 0;
    walk(tree, &top, &mut data, &mut size)?;
    let ctl = control_text(m, size).into_bytes();
    let control = targz(&[Entry::Dir("./".into()), Entry::File("./control".into(), ctl, 0o644)])?;
    let data = targz(&data)?;
    Ok(ar(&[("debian-binary", b"2.0\n"), ("control.tar.gz", &control), ("data.tar.gz", &data)]))
}

fn failure(message: String) -> Value {
    json!({ "ok": false, "message": message })
}

/// The release folder next to the archive (build.rs finalize_release): <root>/<BASE>/, whose content level
/// is <BASE>/<BASE>/ when the release was a single folder. Returns (tree, folder name).
fn release_tree(archive_path: &Path) -> Option<(PathBuf, String)> {
    let base = archive_path.file_stem()?.to_string_lossy().to_string();
    let dir = archive_path.with_file_name(&base);
    let inner = dir.join(&base);
    if inner.is_dir() {
        Some((inner, base))
    } else {
        dir.is_dir().then_some((dir, base))
    }
}

/// Empties the local feed mirror of an earlier run, so its Packages lists only the feed's packages and this one.
fn clear_mirror(out: &Path) -> std::io::Result<()> {
    fs::create_dir_all(out)?;
    for e in fs::read_dir(out)?.filter_map(|e| e.ok()) {
        let n = e.file_name().to_string_lossy().into_owned();
        if n.ends_with(".ipk") || n == "Packages" || n == "Packages.gz" {
            fs::remove_file(e.path())?;
        }
    }
    Ok(())
}

/// Never fails: like the Aminet upload, a failed feed publish is reported next to the other results.
/// `run` runs the gh CLI (a fake in tests). The flow of the spec: mirror the feed's .ipk files into
/// <archive dir>/upkg, replace this package's older file, index the mirror, then upload the .ipk first,
/// Packages and Packages.gz next, and delete the superseded asset last. A dry run stops after the index.
pub fn publish_upkg_core(request: &Value, archive_path: &Path, run: &dyn Fn(&str, &[String]) -> Result<(), String>) -> Value {
    let m = match manifest_from_request(request) {
        Ok(m) => m,
        Err(p) => return failure(format!("not published to upkg: {}", p.join("; "))),
    };
    let dry_run = request["dryRun"].as_bool().unwrap_or(true);
    let Some((tree, folder)) = release_tree(archive_path) else {
        return failure(format!("not published to upkg: the release folder next to {} is missing", archive_path.display()));
    };
    let ipk = match build_ipk(&m, &tree, &folder) {
        Ok(b) => b,
        Err(e) => return failure(format!("not published to upkg: {e}")),
    };
    let out = archive_path.with_file_name("upkg");
    if let Err(e) = clear_mirror(&out) {
        return failure(format!("not published to upkg: could not prepare {}: {e}", out.display()));
    }
    let s = |v: &[&str]| v.iter().map(|x| x.to_string()).collect::<Vec<_>>();
    let out_str = out.display().to_string();
    // The repository is the owner's to create; a missing one is reported, never created here.
    let feed = run("gh", &s(&["release", "view", FEED_TAG, "-R", FEED_REPO])).and_then(|()| {
        run("gh", &s(&["release", "download", FEED_TAG, "-R", FEED_REPO, "-p", "*.ipk", "-D", &out_str, "--clobber"]))
            .or_else(|e| if e.contains("no assets") { Ok(()) } else { Err(e) })
    });
    if let Err(e) = feed {
        if !dry_run {
            return failure(format!(
                "not published to upkg: the feed {FEED_REPO} (release {FEED_TAG}) is missing or cannot be read. The owner creates it once: \
                 gh repo create {FEED_REPO} --public, then gh release create {FEED_TAG} --repo {FEED_REPO} --latest. ({e})"
            ));
        }
    }
    // This package's files already in the feed: the same name is overwritten, an older upstream version is stale.
    let mut stale = Vec::new();
    for e in fs::read_dir(&out).into_iter().flatten().filter_map(|e| e.ok()) {
        let n = e.file_name().to_string_lossy().into_owned();
        let Ok(c) = fs::read(e.path()).map_err(|e| e.to_string()).and_then(|d| ipk_control(&d)).and_then(|t| parse_control(&t)) else { continue };
        if field(&c, "Package") == m.package && field(&c, "Architecture") == ARCH && n.ends_with(".ipk") {
            if n != m.ipk_name() {
                stale.push(n);
            }
            let _ = fs::remove_file(e.path());
        }
    }
    let ipk_path = out.join(m.ipk_name());
    let packages_path = out.join("Packages");
    let gz_path = out.join("Packages.gz");
    let written = fs::write(&ipk_path, &ipk).map_err(|e| e.to_string()).and_then(|()| feed_index(&out)).and_then(|p| {
        let gz = gzip(p.as_bytes())?;
        fs::write(&packages_path, &p).and_then(|()| fs::write(&gz_path, gz)).map_err(|e| e.to_string())
    });
    if let Err(e) = written {
        return failure(format!("not published to upkg: could not write the feed lists in {out_str}: {e}"));
    }
    let paths = json!({ "ipkPath": ipk_path.display().to_string(), "packagesPath": packages_path.display().to_string(), "dryRun": dry_run });
    let mut result = if dry_run {
        json!({ "ok": true, "message": format!("dry run: wrote {} and the feed lists in {out_str}; nothing uploaded", m.ipk_name()) })
    } else {
        let upload = |files: &[&Path]| {
            let mut args = s(&["release", "upload", FEED_TAG]);
            args.extend(files.iter().map(|f| f.display().to_string()));
            args.extend(s(&["-R", FEED_REPO, "--clobber"]));
            run("gh", &args)
        };
        // .ipk first, so a client never reads a Packages entry whose file is not there yet
        match upload(&[&ipk_path]).and_then(|()| upload(&[&packages_path, &gz_path])) {
            Err(e) => failure(format!("upkg feed upload failed: {e}")),
            Ok(()) => {
                let left: Vec<String> = stale
                    .iter()
                    .filter(|n| run("gh", &s(&["release", "delete-asset", FEED_TAG, n, "-R", FEED_REPO, "--yes"])).is_err())
                    .cloned()
                    .collect();
                let note = if left.is_empty() { String::new() } else { format!(" (could not delete the old {}; delete it on GitHub)", left.join(", ")) };
                json!({ "ok": true, "message": format!("published {} {} to the upkg feed {FEED_REPO}{note}", m.package, m.full_version()) })
            }
        }
    };
    for (k, v) in paths.as_object().unwrap() {
        result[k] = v.clone();
    }
    result
}

fn gh(bin: &str, args: &[String]) -> Result<(), String> {
    crate::build::run_tool(bin, args, None, "install the GitHub CLI (\"brew install gh\") and run \"gh auth login\" or set GH_TOKEN")
}

#[tauri::command]
pub async fn publish_upkg(request: Value, archive_path: String) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || publish_upkg_core(&request, Path::new(&archive_path), &gh)).await.map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    // Written by vtcon tools/pkgs.py (19ae3a01) build-ipk and feed-index from the tree of demo_tree(); hello is
    // the sample package of vtcon's dry-run feed.
    const PKGS_PY_IPK: &[u8] = include_bytes!("../testdata/upkg/dropdemo_1.0_m68k-amigaos.ipk");
    const PKGS_PY_HELLO: &[u8] = include_bytes!("../testdata/upkg/hello_1.0_all.ipk");
    const PKGS_PY_PACKAGES: &str = include_str!("../testdata/upkg/Packages");
    const PKGS_PY_PACKAGES_GZ: &[u8] = include_bytes!("../testdata/upkg/Packages.gz");
    const LONG: &str = "a-long-drawer-name-number-one/another-long-drawer-name-two/third-quite-long-drawer-name";

    fn req(dry: bool) -> Value {
        json!({ "package": "dropdemo", "version": "1.0", "revision": 2, "description": "Release Dropper demo package",
                "maintainer": "UpRough <https://github.com/spotUP>", "installDrawer": "Work:Utilities", "dryRun": dry })
    }

    /// <t>/UP-DEMO.LHA and its release folder <t>/UP-DEMO/, the stage of the pkgs.py fixture below Work/Utilities.
    fn demo_tree() -> (tempfile::TempDir, PathBuf) {
        let t = tempfile::tempdir().unwrap();
        let d = t.path().join("UP-DEMO");
        fs::create_dir_all(d.join("Libs")).unwrap();
        fs::create_dir_all(d.join(LONG)).unwrap();
        fs::write(d.join("Libs/muimaster.library"), b"lib\n").unwrap();
        fs::write(d.join("ReadMe"), b"read me\n").unwrap();
        fs::write(d.join("run"), b"#!/bin/sh\necho hi\n").unwrap();
        #[cfg(unix)]
        fs::set_permissions(d.join("run"), std::os::unix::fs::PermissionsExt::from_mode(0o755)).unwrap();
        fs::write(d.join(LONG).join("last-file-with-a-long-name.txt"), b"deep\n").unwrap();
        let archive = t.path().join("UP-DEMO.LHA");
        fs::write(&archive, b"lha").unwrap();
        (t, archive)
    }

    #[test]
    fn manifest_rejects_bad_fields() {
        let mut r = req(true);
        r["package"] = json!("Open MUI");
        r["version"] = json!("1.0-beta");
        r["installDrawer"] = json!("Utilities");
        let p = manifest_from_request(&r).unwrap_err();
        assert_eq!(p.len(), 3, "{p:?}");
        assert_eq!(manifest_from_request(&req(true)).unwrap().full_version(), "1.0-r2");
    }

    #[test]
    fn manifest_enforces_name_rules() {
        let with = |k: &str, v: &str| {
            let mut r = req(true);
            r[k] = json!(v);
            manifest_from_request(&r)
        };
        assert!(with("package", "up-demo").unwrap_err()[0].contains("up-"));
        assert!(with("package", "a23456789012345678901").is_err(), "21 characters");
        assert!(with("package", "a2345678901234567890").is_err(), "20 is allowed, but the file name is then 41");
        let e = with("version", "1.0.0.0.0.0.0").unwrap_err();
        assert!(e[0].contains("dropdemo_1.0.0.0.0.0.0_m68k-amigaos.ipk is 39 characters"), "{e:?}");
        assert!(with("installDrawer", "Work:a234567890123456789012345678901").is_err(), "31-character drawer");
        assert_eq!(manifest_from_request(&req(true)).unwrap().ipk_name(), "dropdemo_1.0_m68k-amigaos.ipk");
    }

    #[cfg(unix)]
    #[test]
    fn ipk_is_byte_equal_to_pkgs_py() {
        let (_t, archive) = demo_tree();
        let m = manifest_from_request(&req(true)).unwrap();
        let (tree, folder) = release_tree(&archive).unwrap();
        let ipk = build_ipk(&m, &tree, &folder).unwrap();
        let mem = split_ar(&ipk).unwrap();
        let want = split_ar(PKGS_PY_IPK).unwrap();
        assert_eq!(mem.iter().map(|(n, _)| n.as_str()).collect::<Vec<_>>(), ["debian-binary", "control.tar.gz", "data.tar.gz"]);
        // the tar bytes first, so a mismatch says whether the tar or the gzip differs
        for i in 1..3 {
            let unz = |b: &[u8]| { let mut v = Vec::new(); flate2::read::GzDecoder::new(b).read_to_end(&mut v).unwrap(); v };
            assert!(unz(&mem[i].1) == unz(&want[i].1), "{} tar differs", mem[i].0);
        }
        assert!(ipk == PKGS_PY_IPK, "gzip bytes differ from pkgs.py");
    }

    #[test]
    fn walk_refuses_long_names() {
        let (t, archive) = demo_tree();
        fs::write(t.path().join("UP-DEMO/a234567890123456789012345678901"), b"x").unwrap();
        let (tree, folder) = release_tree(&archive).unwrap();
        let e = build_ipk(&manifest_from_request(&req(true)).unwrap(), &tree, &folder).err().unwrap();
        assert!(e.contains("over 30 characters"), "{e}");
    }

    #[test]
    fn feed_index_is_byte_equal_to_pkgs_py() {
        let t = tempfile::tempdir().unwrap();
        fs::write(t.path().join("dropdemo_1.0_m68k-amigaos.ipk"), PKGS_PY_IPK).unwrap();
        fs::write(t.path().join("hello_1.0_all.ipk"), PKGS_PY_HELLO).unwrap();
        let p = feed_index(t.path()).unwrap();
        assert_eq!(p, PKGS_PY_PACKAGES);
        assert!(gzip(p.as_bytes()).unwrap() == PKGS_PY_PACKAGES_GZ, "Packages.gz differs from pkgs.py");
        fs::write(t.path().join("hello_2.0_all.ipk"), PKGS_PY_HELLO).unwrap();
        assert!(feed_index(t.path()).unwrap_err().contains("its control names it hello_1.0_all.ipk"));
    }

    #[test]
    fn dry_run_writes_feed_and_uploads_nothing() {
        let (_t, archive) = demo_tree();
        let calls = RefCell::new(Vec::new());
        let run = |_: &str, a: &[String]| { calls.borrow_mut().push(a.join(" ")); Err("no network in tests".to_string()) };
        let r = publish_upkg_core(&req(true), &archive, &run);
        assert_eq!(r["ok"], true, "{r}");
        assert_eq!(*calls.borrow(), ["release view feed -R spotUP/upkg-feed"], "only the read-only check");
        let pk = fs::read_to_string(r["packagesPath"].as_str().unwrap()).unwrap();
        let ipk = fs::read(r["ipkPath"].as_str().unwrap()).unwrap();
        assert!(pk.starts_with("Package: dropdemo\nVersion: 1.0-r2\n") && pk.contains("Filename: dropdemo_1.0_m68k-amigaos.ipk\n"), "{pk}");
        assert!(pk.contains(&format!("SHA256sum: {}\n", hex(&Sha256::digest(&ipk)))));
    }

    #[test]
    fn real_publish_fails_clearly_when_the_feed_is_missing() {
        let (_t, archive) = demo_tree();
        let calls = RefCell::new(Vec::new());
        let run = |_: &str, a: &[String]| { calls.borrow_mut().push(a.join(" ")); Err("HTTP 404: Not Found".to_string()) };
        let r = publish_upkg_core(&req(false), &archive, &run);
        assert_eq!(r["ok"], false);
        assert!(r["message"].as_str().unwrap().contains("the feed spotUP/upkg-feed (release feed) is missing"), "{r}");
        assert_eq!(calls.borrow().len(), 1, "nothing created or uploaded");
    }

    #[test]
    fn real_publish_uploads_ipk_then_lists_then_deletes_stale() {
        let (_t, archive) = demo_tree();
        // the feed holds hello and an older upstream version of this package
        let mut old = manifest_from_request(&req(true)).unwrap();
        old.version = "0.9".into();
        let (tree, folder) = release_tree(&archive).unwrap();
        let old_ipk = build_ipk(&old, &tree, &folder).unwrap();
        let calls = RefCell::new(Vec::new());
        let run = |_: &str, a: &[String]| {
            calls.borrow_mut().push(a.to_vec());
            if a[1] == "download" {
                let d = Path::new(&a[a.iter().position(|x| x == "-D").unwrap() + 1]);
                fs::write(d.join("hello_1.0_all.ipk"), PKGS_PY_HELLO).unwrap();
                fs::write(d.join(old.ipk_name()), &old_ipk).unwrap();
            }
            Ok(())
        };
        let r = publish_upkg_core(&req(false), &archive, &run);
        assert_eq!(r["ok"], true, "{r}");
        let c = calls.borrow();
        assert_eq!(c.len(), 5, "{c:?}");
        assert_eq!(&c[2][..3], ["release", "upload", "feed"]);
        assert!(c[2][3].ends_with("dropdemo_1.0_m68k-amigaos.ipk") && c[2][4..] == ["-R", "spotUP/upkg-feed", "--clobber"]);
        assert!(c[3][3].ends_with("Packages") && c[3][4].ends_with("Packages.gz") && c[3][5..] == ["-R", "spotUP/upkg-feed", "--clobber"]);
        assert_eq!(c[4], ["release", "delete-asset", "feed", "dropdemo_0.9_m68k-amigaos.ipk", "-R", "spotUP/upkg-feed", "--yes"]);
        let pk = fs::read_to_string(r["packagesPath"].as_str().unwrap()).unwrap();
        assert!(pk.contains("Filename: dropdemo_1.0_m68k-amigaos.ipk\n") && pk.contains("Filename: hello_1.0_all.ipk\n") && !pk.contains("0.9"), "{pk}");
    }
}
