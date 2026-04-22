# Contributing to FinceptTerminal

Thank you for contributing! Please follow these guidelines.

## How to Contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing`)
3. Make your changes
4. Run tests: `pytest`
5. Commit (`git commit -m 'Add amazing feature'`)
6. Push (`git push origin feature/amazing`)
7. Open a Pull Request

## Development Setup

```bash
pip install -e .
fincept-terminal
```

## Code Style
- Python 3.11+
- Use type hints
- Follow PEP 8 (max line length: 120)
- Write docstrings for public functions

## Financial Data
- Never include API keys in code
- Use environment variables for credentials
- Respect rate limits of data providers

## License
By contributing, you agree your contributions will be licensed under the MIT License.
