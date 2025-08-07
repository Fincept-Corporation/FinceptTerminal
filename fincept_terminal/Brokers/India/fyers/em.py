#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import unicodedata


def remove_emojis_from_file():
    """
    Remove all emojis from fyers_tab.py file and save the cleaned version
    """

    input_file = "fyers_tab.py"
    output_file = "fyers_tab_cleaned.py"

    # Comprehensive emoji pattern that covers:
    # - Standard emojis (U+1F600-U+1F64F, U+1F300-U+1F5FF, U+1F680-U+1F6FF, U+1F1E0-U+1F1FF)
    # - Symbols & Pictographs (U+1F900-U+1F9FF, U+2600-U+26FF, U+2700-U+27BF)
    # - Miscellaneous symbols (U+1F000-U+1F02F, U+1F0A0-U+1F0FF)
    # - Additional emoji ranges
    emoji_pattern = re.compile(
        "["
        "\U0001F600-\U0001F64F"  # emoticons
        "\U0001F300-\U0001F5FF"  # symbols & pictographs
        "\U0001F680-\U0001F6FF"  # transport & map symbols
        "\U0001F1E0-\U0001F1FF"  # flags (iOS)
        "\U0001F900-\U0001F9FF"  # Supplemental Symbols and Pictographs
        "\U00002600-\U000026FF"  # Miscellaneous Symbols
        "\U00002700-\U000027BF"  # Dingbats
        "\U0001F000-\U0001F02F"  # Mahjong Tiles
        "\U0001F0A0-\U0001F0FF"  # Playing Cards
        "\U0000FE00-\U0000FE0F"  # Variation Selectors
        "\U0001F170-\U0001F251"  # Enclosed characters
        "\U00003030"  # Wavy dash
        "\U0000303D"  # Part alternation mark
        "\U00003297"  # Circled ideograph congratulation
        "\U00003299"  # Circled ideograph secret
        "\U000000A9"  # Copyright sign
        "\U000000AE"  # Registered sign
        "\U0000203C"  # Double exclamation mark
        "\U00002049"  # Exclamation question mark
        "\U00002122"  # Trade mark sign
        "\U00002139"  # Information source
        "\U00002194-\U00002199"  # Arrows
        "\U000021A9-\U000021AA"  # Arrows
        "\U0000231A-\U0000231B"  # Watch
        "\U00002328"  # Keyboard
        "\U000023CF"  # Eject symbol
        "\U000023E9-\U000023F3"  # Media controls
        "\U000023F8-\U000023FA"  # Media controls
        "\U000024C2"  # Circled M
        "\U000025AA-\U000025AB"  # Squares
        "\U000025B6"  # Play button
        "\U000025C0"  # Reverse button
        "\U000025FB-\U000025FE"  # Squares
        "\U00002600-\U00002604"  # Weather
        "\U0000260E"  # Telephone
        "\U00002611"  # Ballot box with check
        "\U00002614-\U00002615"  # Weather
        "\U00002618"  # Shamrock
        "\U0000261D"  # Pointing finger
        "\U00002620"  # Skull and crossbones
        "\U00002622-\U00002623"  # Radioactive
        "\U00002626"  # Orthodox cross
        "\U0000262A"  # Star and crescent
        "\U0000262E-\U0000262F"  # Peace symbol
        "\U00002638-\U00002639"  # Wheel of dharma
        "\U0000263A"  # Smiling face
        "\U00002640"  # Female sign
        "\U00002642"  # Male sign
        "\U00002648-\U00002653"  # Zodiac signs
        "\U0000265F-\U00002660"  # Chess
        "\U00002663"  # Club suit
        "\U00002665-\U00002666"  # Heart and diamond suits
        "\U00002668"  # Hot springs
        "\U0000267B"  # Recycling symbol
        "\U0000267E-\U0000267F"  # Infinity and wheelchair
        "\U00002692-\U00002697"  # Tools
        "\U00002699"  # Gear
        "\U0000269B-\U0000269C"  # Atom symbol
        "\U000026A0-\U000026A1"  # Warning signs
        "\U000026AA-\U000026AB"  # Circles
        "\U000026B0-\U000026B1"  # Coffin
        "\U000026BD-\U000026BE"  # Sports
        "\U000026C4-\U000026C5"  # Weather
        "\U000026C8"  # Thunder cloud and rain
        "\U000026CE-\U000026CF"  # Ophiuchus
        "\U000026D1"  # Helmet with cross
        "\U000026D3-\U000026D4"  # Chains
        "\U000026E9-\U000026EA"  # Shinto shrine
        "\U000026F0-\U000026F5"  # Mountain and sailing
        "\U000026F7-\U000026FA"  # Skier
        "\U000026FD"  # Fuel pump
        "\U00002702"  # Scissors
        "\U00002705"  # Check mark button
        "\U00002708-\U0000270D"  # Airplane
        "\U0000270F"  # Pencil
        "\U00002712"  # Black nib
        "\U00002714"  # Check mark
        "\U00002716"  # Multiplication sign
        "\U0000271D"  # Latin cross
        "\U00002721"  # Star of David
        "\U00002728"  # Sparkles
        "\U00002733-\U00002734"  # Eight-spoked asterisk
        "\U00002744"  # Snowflake
        "\U00002747"  # Sparkle
        "\U0000274C"  # Cross mark
        "\U0000274E"  # Cross mark button
        "\U00002753-\U00002755"  # Question marks
        "\U00002757"  # Exclamation mark symbol
        "\U00002763-\U00002764"  # Hearts
        "\U00002795-\U00002797"  # Plus and minus
        "\U000027A1"  # Right arrow
        "\U000027B0"  # Curly loop
        "\U000027BF"  # Double curly loop
        "\U00002934-\U00002935"  # Arrows
        "\U00002B05-\U00002B07"  # Arrows
        "\U00002B1B-\U00002B1C"  # Squares
        "\U00002B50"  # Star
        "\U00002B55"  # Circle
        "]+"
    )

    try:
        print(f"Reading file: {input_file}")

        # Read the original file
        with open(input_file, 'r', encoding='utf-8') as file:
            content = file.read()

        print(f"Original file size: {len(content)} characters")

        # Count emojis before removal
        emoji_matches = emoji_pattern.findall(content)
        emoji_count = len(''.join(emoji_matches))

        print(f"Found {emoji_count} emoji characters to remove")

        # Remove emojis
        cleaned_content = emoji_pattern.sub('', content)

        # Also remove any remaining emoji-like characters using unicodedata
        # This catches any emojis that might have been missed by the regex
        final_content = ""
        for char in cleaned_content:
            if unicodedata.category(char) not in ['So', 'Sm']:  # Skip symbols and math symbols that might be emojis
                final_content += char
            elif ord(char) < 128:  # Keep ASCII symbols
                final_content += char

        print(f"Cleaned file size: {len(final_content)} characters")
        print(f"Removed {len(content) - len(final_content)} characters")

        # Write the cleaned content to new file
        with open(output_file, 'w', encoding='utf-8') as file:
            file.write(final_content)

        print(f"✅ Successfully created emoji-free file: {output_file}")

        # Show some statistics
        lines_original = content.count('\n')
        lines_cleaned = final_content.count('\n')

        print(f"\nStatistics:")
        print(f"  Original lines: {lines_original}")
        print(f"  Cleaned lines: {lines_cleaned}")
        print(f"  Characters removed: {len(content) - len(final_content)}")

        # Show a sample of removed emojis (first 20 unique ones)
        unique_emojis = list(set(''.join(emoji_matches)))[:20]
        if unique_emojis:
            print(f"  Sample emojis removed: {''.join(unique_emojis)}")

        return True

    except FileNotFoundError:
        print(f"❌ Error: File '{input_file}' not found!")
        print("Make sure 'fyers_tab.py' exists in the current directory.")
        return False

    except UnicodeDecodeError:
        print(f"❌ Error: Could not read '{input_file}' - encoding issue!")
        print("The file might not be UTF-8 encoded.")
        return False

    except PermissionError:
        print(f"❌ Error: Permission denied!")
        print("Make sure you have write permissions in the current directory.")
        return False

    except Exception as e:
        print(f"❌ Unexpected error: {str(e)}")
        return False


def main():
    """
    Main function to run the emoji removal process
    """
    print("🧹 Emoji Remover for fyers_tab.py")
    print("=" * 40)

    success = remove_emojis_from_file()

    if success:
        print("\n✅ Process completed successfully!")
        print("The cleaned file has been saved as 'fyers_tab_cleaned.py'")
        print("You can now rename it to replace the original file if needed.")
    else:
        print("\n❌ Process failed!")
        print("Please check the error messages above and try again.")


if __name__ == "__main__":
    main()