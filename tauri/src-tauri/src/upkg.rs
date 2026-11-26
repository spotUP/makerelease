// upkg publishing: one .ipk of a built release, added to an upkg (UP-Term's opkg 0.9.0 build) feed.
//
// The format follows UP-Term's own feed builder (vtcon/tools/pkgs.py build_ipk/feed): an ar archive of
// debian-binary "2.0", control.tar.gz and data.tar.gz; Version <upstream>-r<revision>; file name
// <package>_<version>_<arch>.ipk; Packages stanzas with Filename, Size and SHA256sum, which upkg checks.
// UP-Term's upkg.conf has `dest root /` and ixemul reads /Work/x as Work:x, so a data path
// ./Work/Utilities/<BASE>/... installs into Work:Utilities/<BASE>, and upkg removes it again on remove.
// A feed is a GitHub release (upkg reads <repo>/releases/<tag or latest>/download/Packages.gz), written
// with the gh CLI, so the publisher's own `gh auth login` is the only credential.
use flate2::{Compression, GzBuilder};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

/// The one architecture every installed upkg.conf lists (`arch m68k-amigaos 10`).
pub const ARCH: &str = "m68k-amigaos";

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
    pub fn ipk_name(&self) -> String {
        format!("{}_{}_{}.ipk", self.package, self.version, ARCH)
    }
}

fn text(v: &Value, k: &str) -> String {
    v[k].as_str().unwrap_or("").trim().to_string()
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
    // opkg package names: lower case letters, digits, + - . (Debian policy, which opkg follows)
    if m.package.is_empty() || !m.package.starts_with(|c: char| c.is_ascii_lowercase() || c.is_ascii_digit())
        || !m.package.chars().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || "+-.".contains(c))
    {
        p.push("the package name must be lower case letters, digits, + - or . and start with a letter or digit".into());
    }
    // no '-' (it separates the revision) and no '_' (it separates the file name fields)
    if m.version.is_empty() || !m.version.starts_with(|c: char| c.is_ascii_digit())
        || !m.version.chars().all(|c| c.is_ascii_alphanumeric() || ".+~".contains(c))
    {
        p.push("the version must start with a digit and hold only letters, digits, . + or ~".into());
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
        p.push("the install drawer must be an AmigaDOS path such as Work:Utilities".into());
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
    let ok = |s: &str| !s.is_empty() && s != "." && s != ".." && !s.contains(['/', ':', '\\', '\n']);
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

pub fn packages_stanza(m: &Manifest, ipk: &[u8]) -> String {
    format!(
        "Package: {}\nVersion: {}\nArchitecture: {ARCH}\nFilename: {}\nSize: {}\nSHA256sum: {}\nDescription: {}\n",
        m.package,
        m.full_version(),
        m.ipk_name(),
        ipk.len(),
        hex(&Sha256::digest(ipk)),
        m.description
    )
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

/// The feed's Packages text with this package's stanza replacing any older one of the same name and arch.
pub fn merge_packages(old: &str, stanza: &str, package: &str) -> String {
    let field = |s: &str, k: &str| s.lines().find_map(|l| l.strip_prefix(k).map(|v| v.trim().to_string()));
    let mut blocks: Vec<String> = old
        .split("\n\n")
        .map(|b| b.trim_matches('\n'))
        .filter(|b| !b.is_empty())
        .filter(|b| !(field(b, "Package:").as_deref() == Some(package) && field(b, "Architecture:").as_deref() == Some(ARCH)))
        .map(|b| format!("{b}\n"))
        .collect();
    blocks.push(stanza.to_string());
    blocks.join("\n")
}

// ---- deterministic tar.gz and ar, as pkgs.py writes them (mtime 0, uid/gid 0, root/root) ----

enum Entry {
    Dir(String),
    File(String, Vec<u8>, u32),
}

fn octal(field: &mut [u8], v: u64) {
    let s = format!("{:0w$o}", v, w = field.len() - 1);
    field[..s.len()].copy_from_slice(s.as_bytes());
}

fn tar_header(name: &str, size: u64, mode: u32, kind: u8) -> Result<[u8; 512], String> {
    let mut h = [0u8; 512];
    // ustar: a name over 100 bytes is split at a '/' into prefix (155) and name (100)
    let (prefix, base) = if name.len() <= 100 {
        ("", name)
    } else {
        let cut = name[..name.len().min(156)].rfind('/').filter(|&i| name.len() - i - 1 <= 100)
            .ok_or_else(|| format!("path too long for the package: {name}"))?;
        (&name[..cut], &name[cut + 1..])
    };
    h[..base.len()].copy_from_slice(base.as_bytes());
    octal(&mut h[100..108], mode as u64);
    octal(&mut h[108..116], 0);
    octal(&mut h[116..124], 0);
    octal(&mut h[124..136], size);
    octal(&mut h[136..148], 0);
    h[156] = kind;
    h[257..263].copy_from_slice(b"ustar\0");
    h[263..265].copy_from_slice(b"00");
    h[265..269].copy_from_slice(b"root");
    h[297..301].copy_from_slice(b"root");
    h[345..345 + prefix.len()].copy_from_slice(prefix.as_bytes());
    h[148..156].copy_from_slice(b"        ");
    let sum: u32 = h.iter().map(|&b| b as u32).sum();
    let s = format!("{sum:06o}\0 ");
    h[148..156].copy_from_slice(s.as_bytes());
    Ok(h)
}

fn targz(entries: &[Entry]) -> Result<Vec<u8>, String> {
    let mut raw = Vec::new();
    for e in entries {
        match e {
            Entry::Dir(n) => raw.extend(tar_header(n, 0, 0o755, b'5')?),
            Entry::File(n, data, mode) => {
                raw.extend(tar_header(n, data.len() as u64, *mode, b'0')?);
                raw.extend(data);
                raw.resize(raw.len().div_ceil(512) * 512, 0);
            }
        }
    }
    raw.extend([0u8; 1024]);
    let mut gz = GzBuilder::new().mtime(0).write(Vec::new(), Compression::best());
    gz.write_all(&raw).map_err(|e| e.to_string())?;
    gz.finish().map_err(|e| e.to_string())
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

fn walk(dir: &Path, top: &str, out: &mut Vec<Entry>, size: &mut u64) -> Result<(), String> {
    let mut names: Vec<_> = fs::read_dir(dir).map_err(|e| format!("{}: {e}", dir.display()))?.filter_map(|e| e.ok()).collect();
    names.sort_by_key(|e| e.file_name());
    for e in names {
        let name = format!("{top}/{}", e.file_name().to_string_lossy());
        let p = e.path();
        if p.is_dir() {
            out.push(Entry::Dir(format!("{name}/")));
            walk(&p, &name, out, size)?;
        } else {
            let data = fs::read(&p).map_err(|e| format!("{}: {e}", p.display()))?;
            *size += data.len() as u64;
            #[cfg(unix)]
            let mode = if std::os::unix::fs::PermissionsExt::mode(&e.metadata().map_err(|e| e.to_string())?.permissions()) & 0o111 != 0 { 0o755 } else { 0o644 };
            #[cfg(not(unix))]
            let mode = 0o644;
            out.push(Entry::File(name, data, mode));
        }
    }
    Ok(())
}

/// The .ipk bytes: the files of `tree` under <install drawer>/<folder>/.
pub fn build_ipk(m: &Manifest, tree: &Path, folder: &str) -> Result<Vec<u8>, String> {
    let parts = data_prefix(&m.install_drawer).ok_or("bad install drawer")?;
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

fn gzip(b: &[u8]) -> Result<Vec<u8>, String> {
    let mut gz = GzBuilder::new().mtime(0).write(Vec::new(), Compression::best());
    gz.write_all(b).map_err(|e| e.to_string())?;
    gz.finish().map_err(|e| e.to_string())
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

/// Never fails: like the Aminet upload, a failed feed publish is reported next to the other results.
/// `run` runs the gh CLI (a fake in tests). A dry run writes the .ipk and the merged Packages next to the
/// archive and uploads nothing.
pub fn publish_upkg_core(request: &Value, archive_path: &Path, run: &dyn Fn(&str, &[String]) -> Result<(), String>) -> Value {
    let m = match manifest_from_request(request) {
        Ok(m) => m,
        Err(p) => return failure(format!("not published to upkg: {}", p.join("; "))),
    };
    let repo = text(request, "repo");
    let tag = text(request, "tag");
    if repo.split('/').filter(|s| !s.is_empty()).count() != 2 || tag.is_empty() {
        return failure("not published to upkg: the feed needs a GitHub repository (owner/name) and a release tag".into());
    }
    let dry_run = request["dryRun"].as_bool().unwrap_or(true);
    let Some((tree, folder)) = release_tree(archive_path) else {
        return failure(format!("not published to upkg: the release folder next to {} is missing", archive_path.display()));
    };
    let out = archive_path.with_file_name("upkg");
    let ipk = match build_ipk(&m, &tree, &folder) {
        Ok(b) => b,
        Err(e) => return failure(format!("not published to upkg: {e}")),
    };
    let ipk_path = out.join(m.ipk_name());
    let packages_path = out.join("Packages");
    let _ = fs::remove_file(&packages_path);
    if let Err(e) = fs::create_dir_all(&out).and_then(|_| fs::write(&ipk_path, &ipk)) {
        return failure(format!("not published to upkg: could not write {}: {e}", ipk_path.display()));
    }
    let s = |v: &[&str]| v.iter().map(|x| x.to_string()).collect::<Vec<_>>();
    let out_str = out.display().to_string();
    // The feed's current list, so the other packages in it stay listed. A new feed has none.
    if let Err(e) = run("gh", &s(&["release", "download", &tag, "-R", &repo, "-p", "Packages", "-D", &out_str, "--clobber"])) {
        if !dry_run && !e.contains("no assets") {
            return failure(format!("not published to upkg: could not read the feed's Packages: {e}"));
        }
    }
    let old = fs::read_to_string(&packages_path).unwrap_or_default();
    let packages = merge_packages(&old, &packages_stanza(&m, &ipk), &m.package);
    let gz_path = out.join("Packages.gz");
    if let Err(e) = fs::write(&packages_path, &packages).and_then(|_| gzip(packages.as_bytes()).map_err(std::io::Error::other)).and_then(|g| fs::write(&gz_path, g)) {
        return failure(format!("not published to upkg: could not write {}: {e}", packages_path.display()));
    }
    let paths = json!({ "ipkPath": ipk_path.display().to_string(), "packagesPath": packages_path.display().to_string(), "dryRun": dry_run });
    let mut result = if dry_run {
        json!({ "ok": true, "message": format!("dry run: wrote {} and the feed lists in {out_str}; nothing uploaded", m.ipk_name()) })
    } else {
        let files = [ipk_path.display().to_string(), packages_path.display().to_string(), gz_path.display().to_string()];
        let mut args = s(&["release", "upload", &tag, "-R", &repo]);
        args.extend(files);
        args.push("--clobber".into());
        match run("gh", &args) {
            Ok(()) => json!({ "ok": true, "message": format!("published {} {} to the upkg feed {repo} {tag}", m.package, m.full_version()) }),
            Err(e) => failure(format!("upkg feed upload failed: {e}")),
        }
    };
    for (k, v) in paths.as_object().unwrap() {
        result[k] = v.clone();
    }
    result
}

fn gh(bin: &str, args: &[String]) -> Result<(), String> {
    crate::build::run_tool(bin, args, None, "install the GitHub CLI (\"brew install gh\") and run \"gh auth login\"")
}

#[tauri::command]
pub async fn publish_upkg(request: Value, archive_path: String) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || publish_upkg_core(&request, Path::new(&archive_path), &gh)).await.map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn req(dry: bool) -> Value {
        json!({ "package": "openmui", "version": "1.0", "revision": 2, "description": "OpenMUI, the MUI replacement",
                "maintainer": "UpRough", "installDrawer": "Work:Utilities", "repo": "owner/feed", "tag": "upkg", "dryRun": dry })
    }

    fn ar_members(b: &[u8]) -> Vec<(String, Vec<u8>)> {
        assert_eq!(&b[..8], b"!<arch>\n");
        let (mut i, mut out) = (8, Vec::new());
        while i < b.len() {
            let h = std::str::from_utf8(&b[i..i + 60]).unwrap();
            let size: usize = h[48..58].trim().parse().unwrap();
            out.push((h[..16].trim().trim_end_matches('/').to_string(), b[i + 60..i + 60 + size].to_vec()));
            i += 60 + size + size % 2;
        }
        out
    }

    fn tar_names(gz: &[u8]) -> Vec<(String, Vec<u8>)> {
        let mut raw = Vec::new();
        std::io::Read::read_to_end(&mut flate2::read::GzDecoder::new(gz), &mut raw).unwrap();
        let (mut i, mut out) = (0, Vec::new());
        while i + 512 <= raw.len() && raw[i] != 0 {
            let h = &raw[i..i + 512];
            let cstr = |s: &[u8]| String::from_utf8(s.iter().take_while(|&&c| c != 0).copied().collect()).unwrap();
            let sum: u32 = h.iter().enumerate().map(|(j, &c)| if (148..156).contains(&j) { 32 } else { c as u32 }).sum();
            assert_eq!(u32::from_str_radix(cstr(&h[148..154]).trim(), 8).unwrap(), sum, "checksum");
            let (pre, name) = (cstr(&h[345..500]), cstr(&h[..100]));
            let size = usize::from_str_radix(cstr(&h[124..135]).trim(), 8).unwrap();
            out.push((if pre.is_empty() { name } else { format!("{pre}/{name}") }, raw[i + 512..i + 512 + size].to_vec()));
            i += 512 + size.div_ceil(512) * 512;
        }
        out
    }

    fn fixture() -> (tempfile::TempDir, PathBuf) {
        let t = tempfile::tempdir().unwrap();
        let inner = t.path().join("UP-OMUI/UP-OMUI/Libs");
        fs::create_dir_all(&inner).unwrap();
        fs::write(inner.join("muimaster.library"), b"lib").unwrap();
        fs::write(t.path().join("UP-OMUI/file_id.diz"), b"diz").unwrap();
        let archive = t.path().join("UP-OMUI.LHA");
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
    fn ipk_layout_matches_upterm_feed_builder() {
        let (_t, archive) = fixture();
        let m = manifest_from_request(&req(true)).unwrap();
        let (tree, folder) = release_tree(&archive).unwrap();
        let ipk = build_ipk(&m, &tree, &folder).unwrap();
        assert_eq!(ipk, build_ipk(&m, &tree, &folder).unwrap(), "byte-equal on a rebuild");
        let mem = ar_members(&ipk);
        assert_eq!(mem.iter().map(|(n, _)| n.as_str()).collect::<Vec<_>>(), ["debian-binary", "control.tar.gz", "data.tar.gz"]);
        assert_eq!(mem[0].1, b"2.0\n");
        let ctl = tar_names(&mem[1].1);
        assert_eq!(String::from_utf8(ctl[1].1.clone()).unwrap(),
            "Package: openmui\nVersion: 1.0-r2\nArchitecture: m68k-amigaos\nMaintainer: UpRough\nInstalled-Size: 1\nDescription: OpenMUI, the MUI replacement\n");
        let names: Vec<_> = tar_names(&mem[2].1).into_iter().map(|(n, _)| n).collect();
        assert_eq!(names, ["./", "./Work/Utilities/", "./Work/Utilities/UP-OMUI/", "./Work/Utilities/UP-OMUI/Libs/", "./Work/Utilities/UP-OMUI/Libs/muimaster.library"]);
    }

    #[test]
    fn merge_replaces_same_package_and_keeps_others() {
        let old = "Package: up-nvim\nVersion: 0.12-r1\nArchitecture: m68k-amigaos\n\nPackage: openmui\nVersion: 1.0-r1\nArchitecture: m68k-amigaos\n";
        let new = merge_packages(old, "Package: openmui\nVersion: 1.0-r2\nArchitecture: m68k-amigaos\n", "openmui");
        assert_eq!(new, "Package: up-nvim\nVersion: 0.12-r1\nArchitecture: m68k-amigaos\n\nPackage: openmui\nVersion: 1.0-r2\nArchitecture: m68k-amigaos\n");
    }

    #[test]
    fn dry_run_writes_feed_and_uploads_nothing() {
        let (_t, archive) = fixture();
        let calls = RefCell::new(Vec::new());
        let run = |_: &str, a: &[String]| { calls.borrow_mut().push(a.join(" ")); Err("no network in tests".to_string()) };
        let r = publish_upkg_core(&req(true), &archive, &run);
        assert_eq!(r["ok"], true, "{r}");
        assert_eq!(calls.borrow().len(), 1, "only the read-only download");
        assert!(calls.borrow()[0].starts_with("release download upkg -R owner/feed"));
        let pk = fs::read_to_string(r["packagesPath"].as_str().unwrap()).unwrap();
        let ipk = fs::read(r["ipkPath"].as_str().unwrap()).unwrap();
        assert!(pk.contains("Filename: openmui_1.0_m68k-amigaos.ipk\n"));
        assert!(pk.contains(&format!("SHA256sum: {}\n", hex(&Sha256::digest(&ipk)))));
    }

    #[test]
    fn real_publish_uploads_ipk_and_lists() {
        let (_t, archive) = fixture();
        let calls = RefCell::new(Vec::new());
        let run = |_: &str, a: &[String]| {
            calls.borrow_mut().push(a.to_vec());
            if a[1] == "download" { Err("no assets match the file pattern".to_string()) } else { Ok(()) }
        };
        let r = publish_upkg_core(&req(false), &archive, &run);
        assert_eq!(r["ok"], true, "{r}");
        let up = &calls.borrow()[1];
        assert_eq!(&up[..5], ["release", "upload", "upkg", "-R", "owner/feed"]);
        assert!(up[5].ends_with("openmui_1.0_m68k-amigaos.ipk") && up[6].ends_with("Packages") && up[7].ends_with("Packages.gz"));
        assert_eq!(up[8], "--clobber");
    }
}
