(() => {
  'use strict';
  const scales = new Map();
  const ids = ['rtPhaseChart','rtDutyChart','rtPeriodChart','rtBemfChart',
    'rtEmfChart','scopePhaseChart','scopeBemfChart'];
  for (const id of ids) {
    const canvas = document.getElementById(id);
    if (!canvas) continue;
    const state = { x: 1, y: 1 };
    scales.set(id, state);
    const bar = document.createElement('div');
    bar.className = 'chart-scale';
    const label = document.createElement('span');
    const updateLabel = () => {
      const title = window.SATLang.pick('SCALE','สเกล');
      label.textContent = `${title}  X ${state.x.toFixed(2)}×  Y ${state.y.toFixed(2)}×`;
    };
    updateLabel();
    window.addEventListener('sat-language-change', updateLabel);
    const controls = [
      ['X−', 'x', 1 / 1.5], ['X+', 'x', 1.5],
      ['Y−', 'y', 1.5], ['Y+', 'y', 1 / 1.5]
    ];
    for (const [text, axis, factor] of controls) {
      const button = document.createElement('button');
      button.type = 'button'; button.textContent = text;
      button.setAttribute('aria-label', `${axis.toUpperCase()} axis ${factor < 1 ? 'zoom in' : 'zoom out'}`);
      button.addEventListener('click', () => {
        state[axis] = Math.max(0.125, Math.min(8, state[axis] * factor));
        updateLabel();
        window.dispatchEvent(new Event('sat-chart-scale'));
      });
      bar.append(button);
    }
    const reset = document.createElement('button');
    reset.type = 'button'; reset.textContent = 'RESET';
    reset.setAttribute('aria-label', 'Reset chart scale');
    reset.addEventListener('click', () => {
      state.x = 1; state.y = 1; updateLabel();
      window.dispatchEvent(new Event('sat-chart-scale'));
    });
    bar.prepend(label); bar.append(reset);
    const parent = canvas.parentElement;
    if (parent.classList.contains('rt-plot')) parent.after(bar);
    else canvas.after(bar);
  }
  window.SATChartScale = { get: (id) => scales.get(id) || { x: 1, y: 1 } };
})();
