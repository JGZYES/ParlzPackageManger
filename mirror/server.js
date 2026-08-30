// PMM 镜像站（网页版，Node.js 无依赖）
//
// 用自带的 Node 运行，或把这些静态文件放到任意 web 服务器（nginx/静态托管）上。
//
//   node server.js                 // 默认 http://localhost:8080
//   PMM_PORT=9000 node server.js   // 自定义端口
//
// 目录约定（见 README）：
//   packages/   registry 索引（packages.json 聚合 + <pkg>.json 单包元数据）
//   files/      实际发布文件（deb/exe/dmg/pdm/tar.gz ...）
//   mirror.ini  提供给 pmm 的镜像配置（server.js 会生成）
//
// 网页 /  /index.html                浏览器浏览、搜索、按平台筛选
//       /api/packages.json            registry 索引
//       /api/packages/<pkg>.json      单包元数据
//       /packages.json                同 /api/packages.json（让 pmm 直接用 / 也行）
//       /files/<name>                 文件下载
//       /mirror.ini                   可直接复制到 ~/.pmm/mirror.ini
//       /upload                       POST 上传（multipart 或 raw）
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const os = require('os');

const ROOT = __dirname;
const PKG_DIR = path.join(ROOT, 'packages');
const PORT = process.env.PMM_PORT ? parseInt(process.env.PMM_PORT, 10) : 8080;
const HOST = process.env.PMM_HOST || '127.0.0.1';
/* Default public base for generated download URLs / mirror.ini.
 * Override with PMM_BASE_URL (e.g. http://127.0.0.1:8080 for local testing). */
const BASE_URL = (process.env.PMM_BASE_URL || 'https://pmm.parlz.com/mirror').replace(/\/+$/, '');

function ensureDirs() {
  if (!fs.existsSync(PKG_DIR)) fs.mkdirSync(PKG_DIR, { recursive: true });
}

/* ---- parse the <pkg>.json latest pointers into an index ---- */
function loadIndex() {
  const index = { generated: new Date().toISOString(), packages: [] };
  if (!fs.existsSync(PKG_DIR)) return index;
  for (const f of fs.readdirSync(PKG_DIR)) {
    if (!f.endsWith('.json')) continue;      /* <pkg>.json only (list, not pkg/ dir) */
    if (fs.statSync(path.join(PKG_DIR, f)).isDirectory()) continue;
    try {
      const meta = JSON.parse(fs.readFileSync(path.join(PKG_DIR, f), 'utf8'));
      if (!meta.variants) continue;
      index.packages.push(meta);
    } catch (e) { /* skip corrupt */ }
  }
  index.packages.sort((a, b) => (a.name || '').localeCompare(b.name || ''));
  return index;
}

/* ---- helpers ---- */
function sha256(p) { return crypto.createHash('sha256').update(p).digest('hex'); }
function sha1(p) { return crypto.createHash('sha1').update(p).digest('hex'); }

function baseUrl() {
  return BASE_URL;
}

/* ---- upload a file into packages/<pkg>/<ver>.pdm, then write registry metadata ---- */
function handleUpload(req, res) {
  let body = [];
  req.on('data', c => body.push(c));
  req.on('end', () => {
    body = Buffer.concat(body);
    let fname = null;
    const ct = req.headers['content-type'] || '';
    if (/multipart\/form-data/.test(ct)) {
      const b = body.toString('latin1');
      const disp = /filename="([^"]+)"/.exec(b);
      fname = disp ? disp[1] : null;
      const marker = `\r\n\r\n`;
      const idx = b.indexOf(marker);
      body = idx >= 0 ? Buffer.from(b.substring(idx + marker.length, b.length - 4 || b.length), 'latin1') : body;
    } else {
      fname = req.headers['x-file-name'] || urlQuery(req.url, 'file') || 'file.pdm';
    }
    if (!fname) { res.writeHead(400); return res.end('no filename'); }
    fname = path.basename(fname.replace(/[\\/]/g, '/'));
    const pkg = (req.headers['x-pkg-name'] || urlQuery(req.url, 'pkg') || '').trim();
    let ver = (req.headers['x-pkg-version'] || urlQuery(req.url, 'ver') || '').trim();
    if (!pkg) { res.writeHead(400); return res.end('no x-pkg-name'); }
    if (!ver) ver = sha256(body).slice(0, 8);

    /* store packages/<pkg>/<ver>-<os>.pdm (os lets one package carry multiple platforms) */
    const dir = path.join(PKG_DIR, pkg);
    fs.mkdirSync(dir, { recursive: true });
    const os = (req.headers['x-pkg-os'] || urlQuery(req.url, 'os') || 'any').trim();
    const storeName = `${ver}-${os}.pdm`;
    const outPath = path.join(dir, storeName);
    fs.writeFileSync(outPath, body);

    const info = {
      size: body.length,
      sha256: sha256(body),
      sha1: sha1(body),
      file: storeName,
      url: `${baseUrl()}/packages/${encodeURIComponent(pkg)}/${encodeURIComponent(storeName)}`,
      os,
      description: (req.headers['x-pkg-desc'] || urlQuery(req.url, 'desc') || `Uploaded via PMM mirror (${baseUrl()})`).trim(),
    };
    writeRegistryEntry(pkg, ver, info);

    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ ok: true, file: storeName, version: ver, size: info.size,
      sha256: info.sha256, sha1: info.sha1, pkg, url: info.url, mirror: baseUrl() }));
  });
}

function writeRegistryEntry(pkg, ver, info) {
  const meta = {
    name: pkg, version: ver,
    file: info.file, url: info.url, sha256: info.sha256,
    os: info.os || 'any',
    description: info.description || `Uploaded via PMM mirror (${baseUrl()})`,
  };
  /* per-version-per-os metadata: packages/<pkg>/<ver>-<os>.json */
  fs.writeFileSync(path.join(PKG_DIR, pkg, `${ver}-${info.os}.json`), JSON.stringify(meta, null, 2));
  /* latest pointer: packages/<pkg>.json with a versions list */
  const latestPath = path.join(PKG_DIR, pkg + '.json');
  let last = {};
  if (fs.existsSync(latestPath)) {
    try { last = JSON.parse(fs.readFileSync(latestPath, 'utf8')); } catch (e) { last = {}; }
  }
  /* variants: one entry per (version, os) so the client can pick by platform */
  let variants = Array.isArray(last.variants) ? last.variants : [];
  variants = variants.filter(v => !(v.version === ver && v.os === (info.os || 'any')));
  variants.push({ version: ver, os: info.os || 'any', file: info.file, url: info.url,
                  sha256: info.sha256, description: info.description || '' });
  const latest = Object.assign({}, meta, { variants });
  fs.writeFileSync(latestPath, JSON.stringify(latest, null, 2));
  console.log(`  [registry] ${pkg} @ ${ver} (os=${meta.os}) -> packages/${pkg}/${ver}-${meta.os}.pdm`);
}

function urlQuery(url, key) {
  const q = url.split('?')[1] || '';
  for (const kv of q.split('&')) {
    const [k, v] = kv.split('=');
    if (k === key) return decodeURIComponent(v || '');
  }
  return null;
}

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json',
  '.ini': 'text/plain', '.txt': 'text/plain', '.css': 'text/css', '.pdm': 'application/octet-stream' };

function serveStatic(res, p) {
  const ext = path.extname(p);
  fs.readFile(p, (err, data) => {
    if (err) { res.writeHead(404); return res.end('not found'); }
    res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream',
                         'Content-Length': data.length }); /* lets the client show total size */
    res.end(data);
  });
}

function generateMirrorIni() {
  return `# 由 PMM 镜像站 ${baseUrl()} 自动生成
# 复制到 ~/.pmm/mirror.ini（或 mirror.conf）即可用 pmm install 从本镜像安装
[main]
registry = ${baseUrl()}/packages
priority = 1
`;
}

const server = http.createServer((req, res) => {
  const u = new URL(req.url, baseUrl());
  const pathname = decodeURIComponent(u.pathname);

  // CORS for browser-based tools
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', '*');
  if (req.method === 'OPTIONS') { res.writeHead(204); return res.end(); }

  if (req.method === 'POST' && pathname === '/upload') return handleUpload(req, res);

  if (pathname === '/' || pathname === '/index.html') return serveStatic(res, path.join(ROOT, 'index.html'));
  if (pathname === '/packages.json' || pathname === '/api/packages.json')
    return serveJson(res, loadIndex());
  if (pathname === '/mirror.ini' || pathname === '/mirror.conf') {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    return res.end(generateMirrorIni());
  }
  const pm = /^\/api\/packages\/([^/]+)\.json$/.exec(pathname) || /^\/packages\/([^/]+)\.json$/.exec(pathname);
  if (pm) {
    const f = path.join(PKG_DIR, pm[1] + '.json');
    if (fs.existsSync(f)) return serveStatic(res, f);
    res.writeHead(404); return res.end('not found');
  }
  // versioned metadata / package file:
  //   /packages/<pkg>/<ver>-<os>.json -> packages/<pkg>/<ver>-<os>.json
  //   /packages/<pkg>/<ver>-<os>.pdm  -> packages/<pkg>/<ver>-<os>.pdm   (download)
  const pv = /^\/api\/packages\/([^/]+)\/([^/]+)\.(json|pdm)$/.exec(pathname) ||
             /^\/packages\/([^/]+)\/([^/]+)\.(json|pdm)$/.exec(pathname);
  if (pv) {
    const ext = '.' + pv[3];
    const f = path.join(PKG_DIR, pv[1], pv[2] + ext);
    if (fs.existsSync(f)) return serveStatic(res, f);
    res.writeHead(404); return res.end('not found');
  }
  if (pathname === '/api/health') { res.writeHead(200); return res.end('ok'); }
  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end('not found');
});

function serveJson(res, obj) { res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(obj, null, 2)); }

ensureDirs();
console.log(`PMM mirror (web) serving ${ROOT}`);
console.log(`  web UI:   ${baseUrl()}/`);
console.log(`  registry: ${baseUrl()}/packages.json`);
console.log(`  mirror.ini: ${baseUrl()}/mirror.ini`);
server.listen(PORT, HOST);
