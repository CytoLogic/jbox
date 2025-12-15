const express = require('express');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// Project root is two levels up from src/pkg_srv/
const PROJECT_ROOT = path.join(__dirname, '..', '..');
const DOWNLOADS_DIR = path.join(PROJECT_ROOT, 'srv', 'pkg_repository', 'downloads');
const MANIFEST_PATH = path.join(PROJECT_ROOT, 'srv', 'pkg_repository', 'pkg_manifest.json');

// Load packages from manifest file
let packages = [];

function loadPackages() {
  try {
    const data = fs.readFileSync(MANIFEST_PATH, 'utf8');
    packages = JSON.parse(data);
    console.log(`Loaded ${packages.length} packages from ${MANIFEST_PATH}`);
  } catch (err) {
    if (err.code === 'ENOENT') {
      console.warn(`Warning: ${MANIFEST_PATH} not found. Run 'make packages' to generate it.`);
      packages = [];
    } else {
      console.error(`Error loading package manifest: ${err.message}`);
      packages = [];
    }
  }
}

// Load packages at startup
loadPackages();

// Middleware for JSON parsing
app.use(express.json());

// Serve package downloads as static files
app.use('/downloads', express.static(DOWNLOADS_DIR));

// Logging middleware
app.use((req, res, next) => {
  console.log(`${new Date().toISOString()} ${req.method} ${req.path}`);
  next();
});

// GET /packages - List all packages
app.get('/packages', (req, res) => {
  res.json({
    status: "ok",
    packages: packages
  });
});

// GET /packages/:name - Get specific package
app.get('/packages/:name', (req, res) => {
  const pkg = packages.find(p => p.name === req.params.name);

  if (pkg) {
    res.json({
      status: "ok",
      package: pkg
    });
  } else {
    res.status(404).json({
      status: "error",
      message: `Package '${req.params.name}' not found`
    });
  }
});

// POST /reload - Reload package manifest (useful for development)
app.post('/reload', (req, res) => {
  loadPackages();
  res.json({
    status: "ok",
    message: `Reloaded ${packages.length} packages`
  });
});

// Start server
app.listen(PORT, () => {
  console.log(`jshell package registry server running on http://localhost:${PORT}`);
  console.log(`Available endpoints:`);
  console.log(`  GET  /packages             - List all packages`);
  console.log(`  GET  /packages/:name       - Get package by name`);
  console.log(`  GET  /downloads/:filename  - Download package tarball`);
  console.log(`  POST /reload               - Reload package manifest`);
  console.log(`Downloads directory: ${DOWNLOADS_DIR}`);
  console.log(`Manifest file: ${MANIFEST_PATH}`);
});
