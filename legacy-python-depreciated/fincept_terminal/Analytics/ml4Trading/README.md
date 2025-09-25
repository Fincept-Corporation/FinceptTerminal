# Enhanced OpenRouter API Client

An improved Python client for the OpenRouter API with robust error handling, retry logic, and security best practices.

## Features

- **Security**: API keys managed through environment variables or secure configuration
- **Error Handling**: Comprehensive error handling with retry logic and rate limiting support
- **Flexibility**: Configurable parameters for timeout, retries, and model selection
- **Type Safety**: Full type hints and dataclass-based configuration
- **Logging**: Detailed logging and response formatting
- **Examples**: Multiple usage examples included

## Installation

Install required dependencies:

```bash
pip install requests
```

## Quick Start

1. **Set up your API key**: Create a `.env` file based on `.env.example`:
   ```
   OPENROUTER_API_KEY=your-api-key-here
   ```

2. **Run the client**:
   ```bash
   python kimik2.py
   ```

## Usage

### Basic Usage

```python
from kimik2 import OpenRouterClient, OpenRouterConfig

# Initialize client (loads API key from environment)
client = OpenRouterClient()

# Send a simple message
messages = [{"role": "user", "content": "Hello, how are you?"}]
response = client.chat_completion(messages)
client.print_chat_response(response)
```

### Advanced Configuration

```python
from kimik2 import OpenRouterClient, OpenRouterConfig

# Custom configuration
config = OpenRouterConfig(
    timeout=60,  # 60 second timeout
    max_retries=5,  # 5 retry attempts
    retry_delay=1.5  # 1.5 second delay between retries
)

client = OpenRouterClient(config=config)

# Send with custom parameters
response = client.chat_completion(
    messages=[{"role": "user", "content": "Tell me about AI"}],
    model="moonshotai/kimi-k2:free",  # Or specify a different model
    max_tokens=100,
    temperature=0.7
)
```

### Direct API Key Usage

```python
# For testing or when environment variables aren't available
client = OpenRouterClient(api_key="your-api-key-here")
```

## Error Handling

The client handles various error scenarios:

- **Network timeouts**: Automatic retry with exponential backoff
- **Rate limiting**: Respects retry-after headers
- **Invalid responses**: Validates API response structure
- **Configuration errors**: Clear error messages for missing inputs

## Configuration

### Environment Variables

- `OPENROUTER_API_KEY`: Your OpenRouter API key (required)

Optional environment variables:
- `OPENROUTER_BASE_URL`: API base URL (default: https://openrouter.ai/api/v1)
- `OPENROUTER_MODEL`: Default model (default: moonshotai/kimi-k2:free)
- `OPENROUTER_TIMEOUT`: Default timeout in seconds

### Configuration Class

```python
@dataclass
class OpenRouterConfig:
    base_url: str = "https://openrouter.ai/api/v1"
    model: str = "moonshotai/kimi-k2:free"
    timeout: int = 30
    max_retries: int = 3
    retry_delay: float = 1.0
```

## API Response Format

The client provides pretty-printed responses showing:
- Timestamp of response
- Model used
- Message content
- Token usage statistics (when available)
- Response validation

## Best Practices

1. **API Key Security**: Always use environment variables for API keys
2. **Error Handling**: Wrap calls in try-except blocks for graceful error handling
3. **Rate Limiting**: Be mindful of API rate limits, the client handles retries automatically
4. **Testing**: Use the built-in examples to verify your setup

## Files

- `kimik2.py`: Main client implementation
- `.env.example`: Example environment configuration
- `README.md`: This documentation