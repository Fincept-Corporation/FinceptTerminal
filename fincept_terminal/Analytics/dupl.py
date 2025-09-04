import pandas as pd

# Read CSV file
df = pd.read_csv('data.csv')

print(f"Original data: {len(df)} rows")

# Remove duplicates based on 'Name' column (keep first occurrence)
df_cleaned = df.drop_duplicates(subset=['Name'], keep='first')

print(f"After removing duplicates: {len(df_cleaned)} rows")
print(f"Removed: {len(df) - len(df_cleaned)} duplicate rows")

# Save cleaned data to new CSV
df_cleaned.to_csv('data_no_duplicates.csv', index=False)

print("\n✅ Cleaned data saved to 'data_no_duplicates.csv'")

# Optional: Show what was removed
removed_count = len(df) - len(df_cleaned)
if removed_count > 0:
    print(f"\n📊 Summary:")
    print(f"   Original entries: {len(df)}")
    print(f"   Unique entries: {len(df_cleaned)}")
    print(f"   Duplicates removed: {removed_count}")