import { createInterface } from 'node:readline/promises';

import { normalizeTemplateName } from './template.mjs';

export const TEMPLATE_CHOICES = Object.freeze([
  {
    name: 'basic',
    label: 'Basic',
    description: 'TypeScript + Vite'
  },
  {
    name: 'react',
    label: 'React',
    description: 'React + TypeScript + Vite'
  },
  {
    name: 'vue',
    label: 'Vue',
    description: 'Vue + TypeScript + Vite'
  },
  {
    name: 'nextjs',
    label: 'Next.js',
    description: 'App Router + static export'
  },
  {
    name: 'nuxtjs',
    label: 'Nuxt',
    description: 'Nuxt + static generation'
  }
]);

export function resolveTemplateChoice(value) {
  const answer = value.trim().toLowerCase();
  if (answer.length === 0) {
    return TEMPLATE_CHOICES[0].name;
  }
  if (/^\d+$/.test(answer)) {
    return TEMPLATE_CHOICES[Number.parseInt(answer, 10) - 1]?.name;
  }
  return normalizeTemplateName(answer);
}

export async function promptTemplate({ skipPrompt = false } = {}) {
  if (skipPrompt || !process.stdin.isTTY || !process.stdout.isTTY) {
    return TEMPLATE_CHOICES[0].name;
  }

  const prompt = createInterface({
    input: process.stdin,
    output: process.stdout
  });
  try {
    process.stdout.write('Select a template:\n');
    for (const [index, choice] of TEMPLATE_CHOICES.entries()) {
      process.stdout.write(
        `  ${index + 1}. ${choice.label} (${choice.description})\n`
      );
    }

    while (true) {
      const answer = await prompt.question(
        `Template [1-${TEMPLATE_CHOICES.length}] (1): `
      );
      const template = resolveTemplateChoice(answer);
      if (template !== undefined) {
        return template;
      }
      process.stdout.write('Enter a template number or name.\n');
    }
  } finally {
    prompt.close();
  }
}
