import requests
import numpy as np
from sentence_transformers import SentenceTransformer
import faiss
import re
from typing import List, Dict, Tuple, Optional
import pickle
from collections import defaultdict


class AdvancedDocumentSearch:
    def __init__(self, model_name: str = "all-mpnet-base-v2"):
        """Enhanced search system with entity awareness"""
        self.model = SentenceTransformer(model_name)
        self.index = None
        self.chunks = []
        self.chunk_metadata = []
        self.full_content = ""
        self.entity_map = {}

    def extract_content(self, url: str) -> str:
        """Extract text content from URL"""
        try:
            headers = {"X-Respond-With": "text"}
            reader_url = f"http://127.0.0.1:4567/{url}"
            response = requests.get(reader_url, headers=headers, timeout=60)
            response.raise_for_status()
            content = response.text
            print(f"Extracted {len(content)} characters")
            return content
        except Exception as e:
            print(f"Error: {e}")
            return ""

    def extract_entities(self, content: str) -> Dict[str, List[str]]:
        """Extract financial entities and relationships"""
        entity_patterns = {
            'companies': r'\b[A-Z]{2,8}(?:\s+Limited|\s+Ltd\.?|\s+Inc\.?|\s+Corp\.?)?\b',
            'amounts': r'Rs\.?\s*[\d,]+(?:\.\d+)?\s*(?:crore|lakh|thousand|million|billion)?',
            'percentages': r'\d+(?:\.\d+)?%',
            'dates': r'(?:March|April|May|June|July|August|September|October|November|December)\s+\d{1,2},?\s+\d{4}|\d{1,2}[-/]\d{1,2}[-/]\d{2,4}',
            'ratings': r'Crisil\s+[A-Z]+\+?(?:/[A-Za-z]+)?'
        }

        entities = defaultdict(set)

        for entity_type, pattern in entity_patterns.items():
            matches = re.findall(pattern, content, re.IGNORECASE)
            for match in matches:
                entities[entity_type].add(match.strip())

        # Convert to regular dict with lists
        return {k: list(v) for k, v in entities.items()}

    def sentence_aware_chunking(self, content: str) -> List[Dict]:
        """Create chunks that preserve sentence and entity boundaries"""
        self.full_content = content
        self.entity_map = self.extract_entities(content)

        chunks = []

        # Split into sentences first
        sentences = re.split(r'(?<=[.!?])\s+', content)

        current_chunk = ""
        current_sentences = []

        for sentence in sentences:
            sentence = sentence.strip()
            if not sentence:
                continue

            # Check if adding this sentence would exceed limit
            test_chunk = current_chunk + " " + sentence if current_chunk else sentence

            if len(test_chunk) > 300 and current_chunk:
                # Save current chunk
                chunks.append({
                    'text': current_chunk.strip(),
                    'type': 'sentence_group',
                    'sentence_count': len(current_sentences),
                    'has_entities': self._has_important_entities(current_chunk)
                })

                current_chunk = sentence
                current_sentences = [sentence]
            else:
                current_chunk = test_chunk
                current_sentences.append(sentence)

        # Add final chunk
        if current_chunk:
            chunks.append({
                'text': current_chunk.strip(),
                'type': 'sentence_group',
                'sentence_count': len(current_sentences),
                'has_entities': self._has_important_entities(current_chunk)
            })

        # Also create entity-focused chunks
        entity_chunks = self._create_entity_chunks(content)
        chunks.extend(entity_chunks)

        print(f"Created {len(chunks)} chunks ({len(entity_chunks)} entity-focused)")
        return chunks

    def _has_important_entities(self, text: str) -> bool:
        """Check if chunk contains important entities"""
        important_patterns = [
            r'\b[A-Z]{3,8}\b',  # Abbreviations like JMFCSL
            r'sponsor|subsidiary|holding|acquisition',
            r'Rs\.?\s*[\d,]+',  # Amounts
            r'\d+(?:\.\d+)?%',  # Percentages
            r'Crisil\s+[A-Z]+'  # Ratings
        ]

        for pattern in important_patterns:
            if re.search(pattern, text, re.IGNORECASE):
                return True
        return False

    def _create_entity_chunks(self, content: str) -> List[Dict]:
        """Create chunks focused on entity relationships"""
        entity_chunks = []

        # Look for relationship patterns
        relationship_patterns = [
            r'[A-Z]{3,8}[^.!?]*(?:sponsor|subsidiary|holding|parent|acquisition|merger)[^.!?]*[.!?]',
            r'[A-Z]{3,8}[^.!?]*(?:Rs\.?\s*[\d,]+)[^.!?]*[.!?]',
            r'[A-Z]{3,8}[^.!?]*(?:\d+(?:\.\d+)?%)[^.!?]*[.!?]',
            r'Rating[^.!?]*[A-Z]{3,8}[^.!?]*[.!?]'
        ]

        for pattern in relationship_patterns:
            matches = re.finditer(pattern, content, re.IGNORECASE | re.DOTALL)
            for match in matches:
                chunk_text = match.group(0).strip()
                if len(chunk_text) > 50:  # Only substantial chunks
                    entity_chunks.append({
                        'text': chunk_text,
                        'type': 'entity_relationship',
                        'sentence_count': 1,
                        'has_entities': True
                    })

        return entity_chunks

    def build_index(self, url: str):
        """Build enhanced search index"""
        content = self.extract_content(url)
        if not content:
            return False

        chunk_data = self.sentence_aware_chunking(content)
        if not chunk_data:
            return False

        self.chunks = [chunk['text'] for chunk in chunk_data]
        self.chunk_metadata = [{k: v for k, v in chunk.items() if k != 'text'} for chunk in chunk_data]

        print("Creating embeddings...")
        embeddings = self.model.encode(self.chunks, show_progress_bar=True)

        # Build index with better similarity metric
        dimension = embeddings.shape[1]
        self.index = faiss.IndexFlatIP(dimension)
        faiss.normalize_L2(embeddings)
        self.index.add(embeddings.astype('float32'))

        print(f"Index ready with {len(self.chunks)} chunks")
        return True

    def enhanced_exact_search(self, query: str) -> List[Tuple[str, float, str]]:
        """Enhanced exact search with entity awareness"""
        results = []
        query_lower = query.lower()

        # Extract entities from query
        query_entities = re.findall(r'\b[A-Z]{2,8}\b', query)

        # Search in full content with context
        content_lower = self.full_content.lower()

        # Try exact phrase first
        if query_lower in content_lower:
            start_idx = content_lower.find(query_lower)
            context = self._extract_context(start_idx, len(query))
            results.append((context, 1.0, "exact_phrase"))

        # Search for entity relationships
        if query_entities:
            for entity in query_entities:
                entity_pattern = rf'\b{re.escape(entity)}\b[^.!?]*[.!?]'
                matches = re.finditer(entity_pattern, self.full_content, re.IGNORECASE)

                for match in matches:
                    sentence = match.group(0).strip()
                    # Check if sentence is relevant to query intent
                    if self._is_relevant_to_query(sentence, query):
                        results.append((sentence, 0.9, f"entity_match_{entity}"))

        # Keyword-based search with better scoring
        query_words = [w for w in query_lower.split() if len(w) > 2]

        for chunk in self.chunks:
            chunk_lower = chunk.lower()
            word_matches = sum(1 for word in query_words if word in chunk_lower)

            if word_matches >= len(query_words) * 0.6:  # At least 60% of words match
                score = word_matches / len(query_words)
                results.append((chunk, score * 0.8, "keyword_match"))

        return results

    def _extract_context(self, start_idx: int, query_length: int) -> str:
        """Extract relevant context around found text"""
        # Find sentence boundaries
        text_before = self.full_content[:start_idx]
        text_after = self.full_content[start_idx + query_length:]

        # Find start of current sentence
        sentence_start = text_before.rfind('.', max(0, len(text_before) - 200))
        if sentence_start == -1:
            sentence_start = max(0, len(text_before) - 200)

        # Find end of current sentence
        sentence_end = text_after.find('.', 0, min(200, len(text_after)))
        if sentence_end == -1:
            sentence_end = min(200, len(text_after))

        context = (text_before[sentence_start:] +
                   self.full_content[start_idx:start_idx + query_length] +
                   text_after[:sentence_end + 1]).strip()

        return context

    def _is_relevant_to_query(self, sentence: str, query: str) -> bool:
        """Check if sentence is relevant to query intent"""
        sentence_lower = sentence.lower()
        query_lower = query.lower()

        # Check for key relationship words
        if any(word in sentence_lower for word in ['sponsor', 'subsidiary', 'holding', 'parent', 'owns']):
            return True

        # Check for query intent words
        query_intent_words = ['what', 'who', 'which', 'where', 'when', 'how much', 'sponsor of']
        for intent in query_intent_words:
            if intent in query_lower:
                return True

        return False

    def semantic_search(self, query: str, top_k: int = 5) -> List[Tuple[str, float, str]]:
        """Enhanced semantic search"""
        if not self.index:
            return []

        query_embedding = self.model.encode([query])
        faiss.normalize_L2(query_embedding)

        scores, indices = self.index.search(query_embedding.astype('float32'), top_k * 2)

        results = []
        for score, idx in zip(scores[0], indices[0]):
            if idx < len(self.chunks):
                chunk = self.chunks[idx]
                metadata = self.chunk_metadata[idx]

                # Boost scores for entity-rich chunks
                boost = 1.2 if metadata.get('has_entities', False) else 1.0
                boosted_score = float(score) * boost

                results.append((chunk, boosted_score, "semantic"))

        # Sort by boosted score
        results.sort(key=lambda x: x[1], reverse=True)
        return results[:top_k]

    def find_best_answer(self, query: str) -> Optional[str]:
        """Find the most relevant answer"""
        # Get all search results
        exact_results = self.enhanced_exact_search(query)
        semantic_results = self.semantic_search(query, top_k=3)

        # Combine and deduplicate
        all_results = exact_results + semantic_results

        if not all_results:
            return None

        # Remove duplicates and sort by score
        seen_texts = set()
        unique_results = []

        for text, score, method in all_results:
            text_key = text[:100].lower()  # First 100 chars as key
            if text_key not in seen_texts:
                seen_texts.add(text_key)
                unique_results.append((text, score, method))

        # Sort by score
        unique_results.sort(key=lambda x: x[1], reverse=True)

        # Return best result if confidence is high
        if unique_results[0][1] > 0.7:
            return unique_results[0][0]

        # Otherwise return top 2 results
        results = []
        for text, score, method in unique_results[:2]:
            results.append(text)

        return "\n---\n".join(results)

    def search(self, query: str) -> str:
        """Main search function"""
        result = self.find_best_answer(query)

        if result:
            return result
        else:
            return f"No relevant information found for: {query}"

    def save_index(self, filename: str = "advanced_search_index"):
        """Save index"""
        if self.index and self.chunks:
            faiss.write_index(self.index, f"{filename}.faiss")
            with open(f"{filename}_data.pkl", 'wb') as f:
                pickle.dump({
                    'chunks': self.chunks,
                    'metadata': self.chunk_metadata,
                    'full_content': self.full_content,
                    'entity_map': self.entity_map
                }, f)

    def load_index(self, filename: str = "advanced_search_index"):
        """Load index"""
        try:
            self.index = faiss.read_index(f"{filename}.faiss")
            with open(f"{filename}_data.pkl", 'rb') as f:
                data = pickle.load(f)
                self.chunks = data['chunks']
                self.chunk_metadata = data['metadata']
                self.full_content = data.get('full_content', '')
                self.entity_map = data.get('entity_map', {})
            print(f"Loaded {len(self.chunks)} chunks")
            return True
        except Exception as e:
            print(f"Load failed: {e}")
            return False


def main():
    """Interactive search interface"""
    print("Advanced Document Search System")
    print("=" * 50)

    search_system = AdvancedDocumentSearch()

    if not search_system.load_index():
        print("Building new index...")
        url = input("Enter document URL (or press Enter for default): ").strip()
        if not url:
            url = "https://www.crisil.com/mnt/winshare/Ratings/RatingList/RatingDocs/JMFinancialLimited_April%2004_%202025_RR_344382.html"

        if not search_system.build_index(url):
            print("Failed to build index")
            return

        search_system.save_index()

    print("\nSearch ready!")
    print("Try: 'JMFCSL sponsor of what', 'media relations', 'total assets Dec 2024'")
    print("Type 'quit' to exit\n")

    while True:
        try:
            query = input("Query: ").strip()

            if query.lower() in ['quit', 'exit', 'q']:
                break

            if not query:
                continue

            result = search_system.search(query)
            print(f"\nAnswer:\n{result}\n")

        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    main()