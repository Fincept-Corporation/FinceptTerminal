// i18next-parser configuration
export default {
  locales: ['en', 'es', 'fr', 'de', 'zh', 'ja', 'hi', 'ar', 'pt', 'ru', 'bn', 'ko', 'tr', 'it', 'id', 'vi', 'th', 'pl', 'uk', 'ro'],
  defaultNamespace: 'common',
  output: 'public/locales/$LOCALE/$NAMESPACE.json',
  input: ['src/**/*.{ts,tsx}'],
  sort: true,
  createOldCatalogs: false,
  keySeparator: '.',
  namespaceSeparator: ':',
  useKeysAsDefaultValue: false,
  verbose: true,
  failOnWarnings: false,
  customValueTemplate: null,
  resetDefaultValueLocale: null,
  i18nextOptions: null,
  yamlOptions: null
};
