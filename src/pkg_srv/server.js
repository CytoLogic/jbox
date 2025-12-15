const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// Project root is two levels up from src/pkg_srv/
const PROJECT_ROOT = path.join(__dirname, '..', '..');
const DOWNLOADS_DIR = path.join(PROJECT_ROOT, 'srv', 'pkg_repository', 'downloads');

// In-memory package storage
const packages = [
  {
    name: "ls",
    latestVersion: "0.0.1",
    description: "List directory contents",
    downloadUrl: "http://localhost:3000/downloads/ls-0.0.1.tar.gz"
  },
  {
    name: "cat",
    latestVersion: "0.0.1",
    description: "Concatenate and print files",
    downloadUrl: "http://localhost:3000/downloads/cat-0.0.1.tar.gz"
  },
  {
    name: "stat",
    latestVersion: "0.0.1",
    description: "Display file status",
    downloadUrl: "http://localhost:3000/downloads/stat-0.0.1.tar.gz"
  },
  {
    name: "head",
    latestVersion: "0.0.1",
    description: "Output the first part of files",
    downloadUrl: "http://localhost:3000/downloads/head-0.0.1.tar.gz"
  },
  {
    name: "tail",
    latestVersion: "0.0.1",
    description: "Output the last part of files",
    downloadUrl: "http://localhost:3000/downloads/tail-0.0.1.tar.gz"
  },
  {
    name: "cp",
    latestVersion: "0.0.1",
    description: "Copy files and directories",
    downloadUrl: "http://localhost:3000/downloads/cp-0.0.1.tar.gz"
  },
  {
    name: "mv",
    latestVersion: "0.0.1",
    description: "Move or rename files",
    downloadUrl: "http://localhost:3000/downloads/mv-0.0.1.tar.gz"
  },
  {
    name: "rm",
    latestVersion: "0.0.1",
    description: "Remove files or directories",
    downloadUrl: "http://localhost:3000/downloads/rm-0.0.1.tar.gz"
  },
  {
    name: "mkdir",
    latestVersion: "0.0.1",
    description: "Create directories",
    downloadUrl: "http://localhost:3000/downloads/mkdir-0.0.1.tar.gz"
  },
  {
    name: "rmdir",
    latestVersion: "0.0.1",
    description: "Remove empty directories",
    downloadUrl: "http://localhost:3000/downloads/rmdir-0.0.1.tar.gz"
  },
  {
    name: "touch",
    latestVersion: "0.0.1",
    description: "Change file timestamps or create empty files",
    downloadUrl: "http://localhost:3000/downloads/touch-0.0.1.tar.gz"
  },
  {
    name: "rg",
    latestVersion: "0.0.1",
    description: "Search text with regex",
    downloadUrl: "http://localhost:3000/downloads/rg-0.0.1.tar.gz"
  },
  {
    name: "echo",
    latestVersion: "0.0.1",
    description: "Display a line of text",
    downloadUrl: "http://localhost:3000/downloads/echo-0.0.1.tar.gz"
  },
  {
    name: "sleep",
    latestVersion: "0.0.1",
    description: "Delay for a specified time",
    downloadUrl: "http://localhost:3000/downloads/sleep-0.0.1.tar.gz"
  },
  {
    name: "date",
    latestVersion: "0.0.1",
    description: "Display the current date and time",
    downloadUrl: "http://localhost:3000/downloads/date-0.0.1.tar.gz"
  },
  {
    name: "less",
    latestVersion: "0.0.1",
    description: "View file contents page by page",
    downloadUrl: "http://localhost:3000/downloads/less-0.0.1.tar.gz"
  },
  {
    name: "vi",
    latestVersion: "0.0.1",
    description: "Text editor",
    downloadUrl: "http://localhost:3000/downloads/vi-0.0.1.tar.gz"
  },
  {
    name: "pkg",
    latestVersion: "0.0.1",
    description: "Package manager for jshell",
    downloadUrl: "http://localhost:3000/downloads/pkg-0.0.1.tar.gz"
  }
];

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

// Start server
app.listen(PORT, () => {
  console.log(`jshell package registry server running on http://localhost:${PORT}`);
  console.log(`Available endpoints:`);
  console.log(`  GET /packages             - List all packages`);
  console.log(`  GET /packages/:name       - Get package by name`);
  console.log(`  GET /downloads/:filename  - Download package tarball`);
  console.log(`Downloads directory: ${DOWNLOADS_DIR}`);
});
