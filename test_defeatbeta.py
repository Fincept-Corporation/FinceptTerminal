import os
os.environ['PYTHONIOENCODING'] = 'utf-8'

try:
    from defeatbeta_api.data.ticker import Ticker

    print("Testing defeatbeta-api with AAPL...")
    ticker = Ticker('AAPL')

    print("\nFetching earnings call transcripts...")
    transcripts = ticker.earning_call_transcripts()

    print("\nAvailable transcripts:")
    transcript_list = transcripts.get_transcripts_list()
    print(transcript_list[:5] if len(transcript_list) > 5 else transcript_list)

    print("\nFetching latest transcript...")
    if transcript_list:
        latest = transcript_list[0]
        year, quarter = latest['year'], latest['quarter']
        transcript = transcripts.get_transcript(year, quarter)
        print(f"\nTranscript preview (Year: {year}, Q{quarter}):")
        print(str(transcript)[:500])
        print("\n✅ SUCCESS: Earnings transcripts are available!")
    else:
        print("No transcripts found.")

except Exception as e:
    print(f"\n❌ ERROR: {type(e).__name__}: {e}")
