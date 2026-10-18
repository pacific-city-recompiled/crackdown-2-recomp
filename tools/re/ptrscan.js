// Usage: node tools/re/ptrscan.js <guest-image.bin> <funcs.txt> [image base hex] [code start] [code end]
// Lists code addresses that appear as 32-bit big-endian values in the image (vtables, callbacks) but are not
// the start of any generated function. funcs.txt lines: "<start hex> <instr count> <last mnemonic>" (from the
// generated code, see tools/gapscan.sh). Prints "addr refs inside|gap" per candidate.
const fs = require('fs');
const [img, funcsPath] = process.argv.slice(2);
const base = parseInt(process.argv[4] || '82000000', 16);
const codeStart = parseInt(process.argv[5] || '821D0000', 16), codeEnd = parseInt(process.argv[6] || '8304E2F4', 16);
const d = fs.readFileSync(img);
const funcs = fs.readFileSync(funcsPath, 'utf8').trim().split('\n').map(l => { const [s, n] = l.split(' '); const a = parseInt(s, 16); return [a, a + 4 * +n]; }).sort((x, y) => x[0] - y[0]);
const starts = new Set(funcs.map(f => f[0]));
function inside(a) { let lo = 0, hi = funcs.length - 1; while (lo <= hi) { const m = (lo + hi) >> 1; if (funcs[m][0] <= a) lo = m + 1; else hi = m - 1; } return hi >= 0 && a < funcs[hi][1] ? funcs[hi][0] : 0; }
const refs = new Map();
for (let o = 0; o + 4 <= d.length; o += 4) {
  const addr = base + o;
  if (addr >= codeStart && addr < codeEnd) continue;          // only references from data
  const v = d.readUInt32BE(o);
  if (v < codeStart || v >= codeEnd || (v & 3) || starts.has(v)) continue;
  const insn = d.readUInt32BE(v - base);
  if (insn === 0) continue;
  refs.set(v, (refs.get(v) || 0) + 1);
}
for (const [v, n] of [...refs].sort((a, b) => a[0] - b[0])) {
  const f = inside(v);
  console.log(`0x${v.toString(16).toUpperCase()} ${n} ${f ? 'inside:0x' + f.toString(16).toUpperCase() : 'gap'}`);
}
