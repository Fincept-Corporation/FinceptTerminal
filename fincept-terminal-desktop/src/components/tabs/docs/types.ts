export interface DocSection {
  id: string;
  title: string;
  icon: any;
  subsections: DocSubsection[];
}

export interface DocSubsection {
  id: string;
  title: string;
  content: string;
  codeExample?: string;
}

export interface DocCategory {
  id: string;
  name: string;
  icon: any;
}

export type DocSearchResult = {
  sectionId: string;
  subsectionId: string;
  title: string;
  relevance: number;
};
