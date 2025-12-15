const express = require('express');

const app = express();
const PORT = process.env.PORT || 3000;

// In-memory package storage
const packages = [
  {
    name: "hello",
    latestVersion: "1.0.0",
    description: "A simple hello world package",
    downloadUrl: "http://localhost:3000/downloads/hello-1.0.0.tar.gz"
  },
  {
    name: "wcplus",
    latestVersion: "2.1.0",
    description: "Enhanced word count utility",
    downloadUrl: "http://localhost:3000/downloads/wcplus-2.1.0.tar.gz"
  }
];

// Middleware for JSON parsing
app.use(express.json());

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
  console.log(`  GET /packages         - List all packages`);
  console.log(`  GET /packages/:name   - Get package by name`);
});
