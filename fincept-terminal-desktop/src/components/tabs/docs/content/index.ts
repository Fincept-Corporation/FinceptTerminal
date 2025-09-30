import { DocSection } from '../types';
import { finscriptLanguageDoc } from './finscriptLanguage';
import { terminalFeaturesDoc } from './terminalFeatures';
import { tabsReferenceDoc } from './tabsReference';
import { apiReferenceDoc } from './apiReference';
import { tutorialsDoc } from './tutorials';

export const DOC_SECTIONS: DocSection[] = [
  finscriptLanguageDoc,
  tabsReferenceDoc,
  terminalFeaturesDoc,
  apiReferenceDoc,
  tutorialsDoc
];

export * from './finscriptLanguage';
export * from './terminalFeatures';
export * from './tabsReference';
export * from './apiReference';
export * from './tutorials';
