'use strict';

const fs = require('node:fs');
const path = require('node:path');

const appRoot = path.resolve(__dirname, '..');
const webRoot = path.join(__dirname, 'web');
const files = [
  'index.html', 'styles.css', 'app.js', 'i18n.js', 'chart_scale.js',
  'rt_charts.js', 'scope_plot.js', 'somboon-logo.png', 'sat_motor_pilot.ico'
];

fs.mkdirSync(webRoot, { recursive: true });
for (const file of files) fs.copyFileSync(path.join(appRoot, file), path.join(webRoot, file));
console.log(`Prepared ${files.length} SAT motor pilot app files.`);
