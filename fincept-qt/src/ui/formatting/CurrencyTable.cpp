#include "ui/formatting/CurrencyTable.h"

#include <QCoreApplication>
#include <QHash>
#include <QVector>

namespace fincept::ui::formatting {

namespace currency_table_detail {

enum CurrencyTableKind : unsigned { kCtFiat = 0, kCtCrypto = 1, kCtStable = 2 | 1, kCtMetal = 4 };

struct CurrencyTableRow {
    const char* code;
    const char16_t* symbol; // UTF-16 so the glyphs survive MSVC without /utf-8
    const char16_t* plain;  // local symbol; nullptr = symbol
    const char16_t* strict; // unambiguous symbol; nullptr = symbol
    const char* name;       // translated in context "fincept::ui::formatting::Currency"
    int decimals;
    int minor_divisor;
    const char* major; // nullptr = same as code
    bool suffix;
    unsigned kind;
};

#define FCT_NAME(text) QT_TRANSLATE_NOOP("fincept::ui::formatting::Currency", text)

// Symbols: the local glyph where it is unambiguous, otherwise a prefixed one
// (C$, A$, CN¥) or the ISO code (CHF, SEK, AED) — a terminal shows many
// currencies side by side, so "kr" or "$" alone would be ambiguous. The plain
// column holds the local symbol ("$", "¥", "kr") a view may use when it shows
// no other currency sharing it; strict is the form that is unambiguous even
// next to the plain symbols ("US$", "JP¥"). Every `symbol` and every `strict`
// value is unique among the major currencies (the unit test checks it).
// Decimals follow ISO 4217, except IDR (0: no circulating sen).
// clang-format off
const CurrencyTableRow kCurrencyTableRows[] = {
    // Majors and the most traded (BIS triennial survey order, roughly)
    {"USD", u"$",          u"$",  u"US$",       FCT_NAME("US Dollar"),               2, 1, nullptr, false, kCtFiat},
    {"EUR", u"\u20AC",     nullptr, nullptr,    FCT_NAME("Euro"),                    2, 1, nullptr, false, kCtFiat},
    {"JPY", u"\u00A5",     u"\u00A5", u"JP\u00A5", FCT_NAME("Japanese Yen"),         0, 1, nullptr, false, kCtFiat},
    {"GBP", u"\u00A3",     nullptr, nullptr,    FCT_NAME("British Pound"),           2, 1, nullptr, false, kCtFiat},
    {"CNY", u"CN\u00A5",   u"\u00A5", nullptr,  FCT_NAME("Chinese Yuan"),            2, 1, nullptr, false, kCtFiat},
    {"CNH", u"CNH",        u"\u00A5", nullptr,  FCT_NAME("Chinese Yuan (Offshore)"), 2, 1, nullptr, false, kCtFiat},
    {"AUD", u"A$",         u"$",  nullptr,      FCT_NAME("Australian Dollar"),       2, 1, nullptr, false, kCtFiat},
    {"CAD", u"C$",         u"$",  nullptr,      FCT_NAME("Canadian Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"CHF", u"CHF",        nullptr, nullptr,    FCT_NAME("Swiss Franc"),             2, 1, nullptr, false, kCtFiat},
    {"HKD", u"HK$",        u"$",  nullptr,      FCT_NAME("Hong Kong Dollar"),        2, 1, nullptr, false, kCtFiat},
    {"SGD", u"S$",         u"$",  nullptr,      FCT_NAME("Singapore Dollar"),        2, 1, nullptr, false, kCtFiat},
    {"SEK", u"SEK",        u"kr", nullptr,      FCT_NAME("Swedish Krona"),           2, 1, nullptr, false, kCtFiat},
    {"KRW", u"\u20A9",     nullptr, nullptr,    FCT_NAME("South Korean Won"),        0, 1, nullptr, false, kCtFiat},
    {"NOK", u"NOK",        u"kr", nullptr,      FCT_NAME("Norwegian Krone"),         2, 1, nullptr, false, kCtFiat},
    {"NZD", u"NZ$",        u"$",  nullptr,      FCT_NAME("New Zealand Dollar"),      2, 1, nullptr, false, kCtFiat},
    {"INR", u"\u20B9",     nullptr, nullptr,    FCT_NAME("Indian Rupee"),            2, 1, nullptr, false, kCtFiat},
    {"MXN", u"MX$",        u"$",  nullptr,      FCT_NAME("Mexican Peso"),            2, 1, nullptr, false, kCtFiat},
    {"TWD", u"NT$",        u"$",  nullptr,      FCT_NAME("New Taiwan Dollar"),       2, 1, nullptr, false, kCtFiat},
    {"ZAR", u"R",          nullptr, nullptr,    FCT_NAME("South African Rand"),      2, 1, nullptr, false, kCtFiat},
    {"BRL", u"R$",         nullptr, nullptr,    FCT_NAME("Brazilian Real"),          2, 1, nullptr, false, kCtFiat},
    {"DKK", u"DKK",        u"kr", nullptr,      FCT_NAME("Danish Krone"),            2, 1, nullptr, false, kCtFiat},
    {"PLN", u"z\u0142",    nullptr, nullptr,    FCT_NAME("Polish Zloty"),            2, 1, nullptr, false, kCtFiat},
    {"THB", u"\u0E3F",     nullptr, nullptr,    FCT_NAME("Thai Baht"),               2, 1, nullptr, false, kCtFiat},
    {"ILS", u"\u20AA",     nullptr, nullptr,    FCT_NAME("Israeli New Shekel"),      2, 1, nullptr, false, kCtFiat},
    {"IDR", u"Rp",         nullptr, nullptr,    FCT_NAME("Indonesian Rupiah"),       0, 1, nullptr, false, kCtFiat},
    {"CZK", u"K\u010D",    nullptr, nullptr,    FCT_NAME("Czech Koruna"),            2, 1, nullptr, false, kCtFiat},
    {"AED", u"AED",        nullptr, nullptr,    FCT_NAME("UAE Dirham"),              2, 1, nullptr, false, kCtFiat},
    {"TRY", u"\u20BA",     nullptr, nullptr,    FCT_NAME("Turkish Lira"),            2, 1, nullptr, false, kCtFiat},
    {"HUF", u"Ft",         nullptr, nullptr,    FCT_NAME("Hungarian Forint"),        2, 1, nullptr, false, kCtFiat},
    {"CLP", u"CL$",        u"$",  nullptr,      FCT_NAME("Chilean Peso"),            0, 1, nullptr, false, kCtFiat},
    {"SAR", u"SAR",        nullptr, nullptr,    FCT_NAME("Saudi Riyal"),             2, 1, nullptr, false, kCtFiat},
    {"PHP", u"\u20B1",     nullptr, nullptr,    FCT_NAME("Philippine Peso"),         2, 1, nullptr, false, kCtFiat},
    {"MYR", u"RM",         nullptr, nullptr,    FCT_NAME("Malaysian Ringgit"),       2, 1, nullptr, false, kCtFiat},
    {"COP", u"COL$",       u"$",  nullptr,      FCT_NAME("Colombian Peso"),          2, 1, nullptr, false, kCtFiat},
    {"RUB", u"\u20BD",     nullptr, nullptr,    FCT_NAME("Russian Ruble"),           2, 1, nullptr, false, kCtFiat},
    {"RON", u"lei",        nullptr, nullptr,    FCT_NAME("Romanian Leu"),            2, 1, nullptr, false, kCtFiat},
    {"PEN", u"S/",         nullptr, nullptr,    FCT_NAME("Peruvian Sol"),            2, 1, nullptr, false, kCtFiat},
    {"BHD", u"BHD",        nullptr, nullptr,    FCT_NAME("Bahraini Dinar"),          3, 1, nullptr, false, kCtFiat},
    {"BGN", u"BGN",        nullptr, nullptr,    FCT_NAME("Bulgarian Lev (until 2025)"), 2, 1, nullptr, false, kCtFiat},
    {"ARS", u"AR$",        u"$",  nullptr,      FCT_NAME("Argentine Peso"),          2, 1, nullptr, false, kCtFiat},
    {"ISK", u"ISK",        u"kr", nullptr,      FCT_NAME("Icelandic Krona"),         0, 1, nullptr, false, kCtFiat},
    {"KWD", u"KWD",        nullptr, nullptr,    FCT_NAME("Kuwaiti Dinar"),           3, 1, nullptr, false, kCtFiat},
    {"QAR", u"QAR",        nullptr, nullptr,    FCT_NAME("Qatari Riyal"),            2, 1, nullptr, false, kCtFiat},
    {"OMR", u"OMR",        nullptr, nullptr,    FCT_NAME("Omani Rial"),              3, 1, nullptr, false, kCtFiat},
    {"JOD", u"JOD",        nullptr, nullptr,    FCT_NAME("Jordanian Dinar"),         3, 1, nullptr, false, kCtFiat},
    {"EGP", u"E\u00A3",    nullptr, nullptr,    FCT_NAME("Egyptian Pound"),          2, 1, nullptr, false, kCtFiat},
    {"NGN", u"\u20A6",     nullptr, nullptr,    FCT_NAME("Nigerian Naira"),          2, 1, nullptr, false, kCtFiat},
    {"PKR", u"Rs",         u"Rs", u"PKR",       FCT_NAME("Pakistani Rupee"),         2, 1, nullptr, false, kCtFiat},
    {"BDT", u"\u09F3",     nullptr, nullptr,    FCT_NAME("Bangladeshi Taka"),        2, 1, nullptr, false, kCtFiat},
    {"NPR", u"NPR",        u"Rs", nullptr,      FCT_NAME("Nepalese Rupee"),          2, 1, nullptr, false, kCtFiat},
    {"LKR", u"LKR",        u"Rs", nullptr,      FCT_NAME("Sri Lankan Rupee"),        2, 1, nullptr, false, kCtFiat},
    {"VND", u"\u20AB",     nullptr, nullptr,    FCT_NAME("Vietnamese Dong"),         0, 1, nullptr, false, kCtFiat},
    {"KZT", u"\u20B8",     nullptr, nullptr,    FCT_NAME("Kazakhstani Tenge"),       2, 1, nullptr, false, kCtFiat},
    {"UAH", u"\u20B4",     nullptr, nullptr,    FCT_NAME("Ukrainian Hryvnia"),       2, 1, nullptr, false, kCtFiat},
    {"MAD", u"MAD",        nullptr, nullptr,    FCT_NAME("Moroccan Dirham"),         2, 1, nullptr, false, kCtFiat},
    {"KES", u"KSh",        nullptr, nullptr,    FCT_NAME("Kenyan Shilling"),         2, 1, nullptr, false, kCtFiat},
    {"TND", u"TND",        nullptr, nullptr,    FCT_NAME("Tunisian Dinar"),          3, 1, nullptr, false, kCtFiat},

    // The rest of ISO 4217 (Yahoo FX pairs and smaller venues). The ISO code is
    // the symbol wherever a glyph would be unfamiliar or clash.
    {"CLF", u"CLF",        nullptr, nullptr,    FCT_NAME("Chilean Unit of Account (UF)"), 4, 1, nullptr, false, kCtFiat},
    {"UYU", u"$U",         u"$",  nullptr,      FCT_NAME("Uruguayan Peso"),          2, 1, nullptr, false, kCtFiat},
    {"VES", u"Bs.",        nullptr, nullptr,    FCT_NAME("Venezuelan Bol\u00EDvar"), 2, 1, nullptr, false, kCtFiat},
    {"IQD", u"IQD",        nullptr, nullptr,    FCT_NAME("Iraqi Dinar"),             3, 1, nullptr, false, kCtFiat},
    {"LYD", u"LYD",        nullptr, nullptr,    FCT_NAME("Libyan Dinar"),            3, 1, nullptr, false, kCtFiat},
    {"XOF", u"CFA",        nullptr, nullptr,    FCT_NAME("West African CFA Franc"),  0, 1, nullptr, false, kCtFiat},
    {"XAF", u"FCFA",       nullptr, nullptr,    FCT_NAME("Central African CFA Franc"), 0, 1, nullptr, false, kCtFiat},
    {"UGX", u"USh",        nullptr, nullptr,    FCT_NAME("Ugandan Shilling"),        0, 1, nullptr, false, kCtFiat},
    {"PYG", u"\u20B2",     nullptr, nullptr,    FCT_NAME("Paraguayan Guaran\u00ED"), 0, 1, nullptr, false, kCtFiat},
    {"RWF", u"RF",         nullptr, nullptr,    FCT_NAME("Rwandan Franc"),           0, 1, nullptr, false, kCtFiat},
    {"GHS", u"GH\u20B5",   nullptr, nullptr,    FCT_NAME("Ghanaian Cedi"),           2, 1, nullptr, false, kCtFiat},
    {"TZS", u"TSh",        nullptr, nullptr,    FCT_NAME("Tanzanian Shilling"),      2, 1, nullptr, false, kCtFiat},
    {"ZMW", u"ZK",         nullptr, nullptr,    FCT_NAME("Zambian Kwacha"),          2, 1, nullptr, false, kCtFiat},
    {"MUR", u"MUR",        u"Rs", nullptr,      FCT_NAME("Mauritian Rupee"),         2, 1, nullptr, false, kCtFiat},
    {"BWP", u"P",          nullptr, nullptr,    FCT_NAME("Botswana Pula"),           2, 1, nullptr, false, kCtFiat},
    {"NAD", u"N$",         u"$",  nullptr,      FCT_NAME("Namibian Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"DZD", u"DZD",        nullptr, nullptr,    FCT_NAME("Algerian Dinar"),          2, 1, nullptr, false, kCtFiat},
    {"RSD", u"RSD",        nullptr, nullptr,    FCT_NAME("Serbian Dinar"),           2, 1, nullptr, false, kCtFiat},
    {"GEL", u"\u20BE",     nullptr, nullptr,    FCT_NAME("Georgian Lari"),           2, 1, nullptr, false, kCtFiat},
    {"MOP", u"MOP$",       u"$",  nullptr,      FCT_NAME("Macanese Pataca"),         2, 1, nullptr, false, kCtFiat},
    {"BND", u"B$",         u"$",  nullptr,      FCT_NAME("Brunei Dollar"),           2, 1, nullptr, false, kCtFiat},
    {"JMD", u"J$",         u"$",  nullptr,      FCT_NAME("Jamaican Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"TTD", u"TT$",        u"$",  nullptr,      FCT_NAME("Trinidad and Tobago Dollar"), 2, 1, nullptr, false, kCtFiat},
    {"DOP", u"RD$",        u"$",  nullptr,      FCT_NAME("Dominican Peso"),          2, 1, nullptr, false, kCtFiat},
    {"CRC", u"\u20A1",     nullptr, nullptr,    FCT_NAME("Costa Rican Col\u00F3n"),  2, 1, nullptr, false, kCtFiat},
    {"GTQ", u"Q",          nullptr, nullptr,    FCT_NAME("Guatemalan Quetzal"),      2, 1, nullptr, false, kCtFiat},
    {"BOB", u"Bs",         nullptr, nullptr,    FCT_NAME("Bolivian Boliviano"),      2, 1, nullptr, false, kCtFiat},
    {"LBP", u"LBP",        nullptr, nullptr,    FCT_NAME("Lebanese Pound"),          2, 1, nullptr, false, kCtFiat},
    {"MNT", u"\u20AE",     nullptr, nullptr,    FCT_NAME("Mongolian T\u00F6gr\u00F6g"), 2, 1, nullptr, false, kCtFiat},
    {"KHR", u"\u17DB",     nullptr, nullptr,    FCT_NAME("Cambodian Riel"),          2, 1, nullptr, false, kCtFiat},
    {"MMK", u"K",          nullptr, nullptr,    FCT_NAME("Myanmar Kyat"),            2, 1, nullptr, false, kCtFiat},
    {"AFN", u"AFN",        nullptr, nullptr,    FCT_NAME("Afghan Afghani"),          2, 1, nullptr, false, kCtFiat},
    {"ALL", u"ALL",        nullptr, nullptr,    FCT_NAME("Albanian Lek"),            2, 1, nullptr, false, kCtFiat},
    {"AMD", u"AMD",        nullptr, nullptr,    FCT_NAME("Armenian Dram"),           2, 1, nullptr, false, kCtFiat},
    {"ANG", u"ANG",        nullptr, nullptr,    FCT_NAME("Netherlands Antillean Guilder"), 2, 1, nullptr, false, kCtFiat},
    {"AOA", u"AOA",        nullptr, nullptr,    FCT_NAME("Angolan Kwanza"),          2, 1, nullptr, false, kCtFiat},
    {"AWG", u"AWG",        nullptr, nullptr,    FCT_NAME("Aruban Florin"),           2, 1, nullptr, false, kCtFiat},
    {"AZN", u"AZN",        nullptr, nullptr,    FCT_NAME("Azerbaijani Manat"),       2, 1, nullptr, false, kCtFiat},
    {"BAM", u"BAM",        nullptr, nullptr,    FCT_NAME("Bosnia-Herzegovina Convertible Mark"), 2, 1, nullptr, false, kCtFiat},
    {"BBD", u"BBD",        u"$",  nullptr,      FCT_NAME("Barbadian Dollar"),        2, 1, nullptr, false, kCtFiat},
    {"BIF", u"BIF",        nullptr, nullptr,    FCT_NAME("Burundian Franc"),         0, 1, nullptr, false, kCtFiat},
    {"BMD", u"BMD",        u"$",  nullptr,      FCT_NAME("Bermudan Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"BSD", u"BSD",        u"$",  nullptr,      FCT_NAME("Bahamian Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"BTN", u"BTN",        nullptr, nullptr,    FCT_NAME("Bhutanese Ngultrum"),      2, 1, nullptr, false, kCtFiat},
    {"BYN", u"BYN",        nullptr, nullptr,    FCT_NAME("Belarusian Ruble"),        2, 1, nullptr, false, kCtFiat},
    {"BZD", u"BZD",        u"$",  nullptr,      FCT_NAME("Belize Dollar"),           2, 1, nullptr, false, kCtFiat},
    {"CDF", u"CDF",        nullptr, nullptr,    FCT_NAME("Congolese Franc"),         2, 1, nullptr, false, kCtFiat},
    {"CUP", u"CUP",        nullptr, nullptr,    FCT_NAME("Cuban Peso"),              2, 1, nullptr, false, kCtFiat},
    {"CVE", u"CVE",        nullptr, nullptr,    FCT_NAME("Cape Verdean Escudo"),     2, 1, nullptr, false, kCtFiat},
    {"DJF", u"DJF",        nullptr, nullptr,    FCT_NAME("Djiboutian Franc"),        0, 1, nullptr, false, kCtFiat},
    {"ERN", u"ERN",        nullptr, nullptr,    FCT_NAME("Eritrean Nakfa"),          2, 1, nullptr, false, kCtFiat},
    {"ETB", u"ETB",        nullptr, nullptr,    FCT_NAME("Ethiopian Birr"),          2, 1, nullptr, false, kCtFiat},
    {"FJD", u"FJD",        u"$",  nullptr,      FCT_NAME("Fijian Dollar"),           2, 1, nullptr, false, kCtFiat},
    {"FKP", u"FKP",        nullptr, nullptr,    FCT_NAME("Falkland Islands Pound"),  2, 1, nullptr, false, kCtFiat},
    {"GIP", u"GIP",        nullptr, nullptr,    FCT_NAME("Gibraltar Pound"),         2, 1, nullptr, false, kCtFiat},
    {"GMD", u"GMD",        nullptr, nullptr,    FCT_NAME("Gambian Dalasi"),          2, 1, nullptr, false, kCtFiat},
    {"GNF", u"GNF",        nullptr, nullptr,    FCT_NAME("Guinean Franc"),           0, 1, nullptr, false, kCtFiat},
    {"GYD", u"GYD",        u"$",  nullptr,      FCT_NAME("Guyanaese Dollar"),        2, 1, nullptr, false, kCtFiat},
    {"HNL", u"HNL",        nullptr, nullptr,    FCT_NAME("Honduran Lempira"),        2, 1, nullptr, false, kCtFiat},
    {"HTG", u"HTG",        nullptr, nullptr,    FCT_NAME("Haitian Gourde"),          2, 1, nullptr, false, kCtFiat},
    {"IRR", u"IRR",        nullptr, nullptr,    FCT_NAME("Iranian Rial"),            2, 1, nullptr, false, kCtFiat},
    {"KGS", u"KGS",        nullptr, nullptr,    FCT_NAME("Kyrgystani Som"),          2, 1, nullptr, false, kCtFiat},
    {"KMF", u"KMF",        nullptr, nullptr,    FCT_NAME("Comorian Franc"),          0, 1, nullptr, false, kCtFiat},
    {"KPW", u"KPW",        nullptr, nullptr,    FCT_NAME("North Korean Won"),        2, 1, nullptr, false, kCtFiat},
    {"KYD", u"KYD",        u"$",  nullptr,      FCT_NAME("Cayman Islands Dollar"),   2, 1, nullptr, false, kCtFiat},
    {"LAK", u"LAK",        nullptr, nullptr,    FCT_NAME("Laotian Kip"),             2, 1, nullptr, false, kCtFiat},
    {"LRD", u"LRD",        u"$",  nullptr,      FCT_NAME("Liberian Dollar"),         2, 1, nullptr, false, kCtFiat},
    {"LSL", u"LSL",        nullptr, nullptr,    FCT_NAME("Lesotho Loti"),            2, 1, nullptr, false, kCtFiat},
    {"MDL", u"MDL",        nullptr, nullptr,    FCT_NAME("Moldovan Leu"),            2, 1, nullptr, false, kCtFiat},
    {"MGA", u"MGA",        nullptr, nullptr,    FCT_NAME("Malagasy Ariary"),         2, 1, nullptr, false, kCtFiat},
    {"MKD", u"MKD",        nullptr, nullptr,    FCT_NAME("Macedonian Denar"),        2, 1, nullptr, false, kCtFiat},
    {"MRU", u"MRU",        nullptr, nullptr,    FCT_NAME("Mauritanian Ouguiya"),     2, 1, nullptr, false, kCtFiat},
    {"MVR", u"MVR",        nullptr, nullptr,    FCT_NAME("Maldivian Rufiyaa"),       2, 1, nullptr, false, kCtFiat},
    {"MWK", u"MWK",        nullptr, nullptr,    FCT_NAME("Malawian Kwacha"),         2, 1, nullptr, false, kCtFiat},
    {"MZN", u"MZN",        nullptr, nullptr,    FCT_NAME("Mozambican Metical"),      2, 1, nullptr, false, kCtFiat},
    {"NIO", u"NIO",        nullptr, nullptr,    FCT_NAME("Nicaraguan C\u00F3rdoba"), 2, 1, nullptr, false, kCtFiat},
    {"PAB", u"PAB",        nullptr, nullptr,    FCT_NAME("Panamanian Balboa"),       2, 1, nullptr, false, kCtFiat},
    {"PGK", u"PGK",        nullptr, nullptr,    FCT_NAME("Papua New Guinean Kina"),  2, 1, nullptr, false, kCtFiat},
    {"SBD", u"SBD",        u"$",  nullptr,      FCT_NAME("Solomon Islands Dollar"),  2, 1, nullptr, false, kCtFiat},
    {"SCR", u"SCR",        u"Rs", nullptr,      FCT_NAME("Seychellois Rupee"),       2, 1, nullptr, false, kCtFiat},
    {"SDG", u"SDG",        nullptr, nullptr,    FCT_NAME("Sudanese Pound"),          2, 1, nullptr, false, kCtFiat},
    {"SHP", u"SHP",        nullptr, nullptr,    FCT_NAME("St. Helena Pound"),        2, 1, nullptr, false, kCtFiat},
    {"SLE", u"SLE",        nullptr, nullptr,    FCT_NAME("Sierra Leonean Leone"),    2, 1, nullptr, false, kCtFiat},
    {"SOS", u"SOS",        nullptr, nullptr,    FCT_NAME("Somali Shilling"),         2, 1, nullptr, false, kCtFiat},
    {"SRD", u"SRD",        u"$",  nullptr,      FCT_NAME("Surinamese Dollar"),       2, 1, nullptr, false, kCtFiat},
    {"SSP", u"SSP",        nullptr, nullptr,    FCT_NAME("South Sudanese Pound"),    2, 1, nullptr, false, kCtFiat},
    {"STN", u"STN",        nullptr, nullptr,    FCT_NAME("S\u00E3o Tom\u00E9 and Pr\u00EDncipe Dobra"), 2, 1, nullptr, false, kCtFiat},
    {"SYP", u"SYP",        nullptr, nullptr,    FCT_NAME("Syrian Pound"),            2, 1, nullptr, false, kCtFiat},
    {"SZL", u"SZL",        nullptr, nullptr,    FCT_NAME("Swazi Lilangeni"),         2, 1, nullptr, false, kCtFiat},
    {"TJS", u"TJS",        nullptr, nullptr,    FCT_NAME("Tajikistani Somoni"),      2, 1, nullptr, false, kCtFiat},
    {"TMT", u"TMT",        nullptr, nullptr,    FCT_NAME("Turkmenistani Manat"),     2, 1, nullptr, false, kCtFiat},
    {"TOP", u"TOP",        nullptr, nullptr,    FCT_NAME("Tongan Pa\u02BBanga"),     2, 1, nullptr, false, kCtFiat},
    {"UZS", u"UZS",        nullptr, nullptr,    FCT_NAME("Uzbekistani Som"),         2, 1, nullptr, false, kCtFiat},
    {"VUV", u"VUV",        nullptr, nullptr,    FCT_NAME("Vanuatu Vatu"),            0, 1, nullptr, false, kCtFiat},
    {"WST", u"WST",        nullptr, nullptr,    FCT_NAME("Samoan Tala"),             2, 1, nullptr, false, kCtFiat},
    {"XCD", u"XCD",        u"$",  nullptr,      FCT_NAME("East Caribbean Dollar"),   2, 1, nullptr, false, kCtFiat},
    {"XCG", u"XCG",        nullptr, nullptr,    FCT_NAME("Caribbean Guilder"),       2, 1, nullptr, false, kCtFiat},
    {"XPF", u"XPF",        nullptr, nullptr,    FCT_NAME("CFP Franc"),               0, 1, nullptr, false, kCtFiat},
    {"YER", u"YER",        nullptr, nullptr,    FCT_NAME("Yemeni Rial"),             2, 1, nullptr, false, kCtFiat},
    {"ZWG", u"ZWG",        nullptr, nullptr,    FCT_NAME("Zimbabwe Gold"),           2, 1, nullptr, false, kCtFiat},

    // Minor-unit quote currencies (exchange prices quoted in hundredths etc.)
    {"GBp", u"p",          nullptr, nullptr,    FCT_NAME("British Pence"),           2, 100,  "GBP", true, kCtFiat},
    {"GBX", u"p",          nullptr, nullptr,    FCT_NAME("British Pence"),           2, 100,  "GBP", true, kCtFiat},
    {"ZAc", u"c",          nullptr, nullptr,    FCT_NAME("South African Cents"),     0, 100,  "ZAR", true, kCtFiat},
    {"ILA", u"ag.",        nullptr, nullptr,    FCT_NAME("Israeli Agorot"),          0, 100,  "ILS", true, kCtFiat},
    {"KWF", u"fils",       nullptr, nullptr,    FCT_NAME("Kuwaiti Fils"),            0, 1000, "KWD", true, kCtFiat},
    {"USX", u"\u00A2",     nullptr, nullptr,    FCT_NAME("US Cents"),                2, 100,  "USD", true, kCtFiat},

    // Precious metals (one troy ounce)
    {"XAU", u"XAU",        nullptr, nullptr,    FCT_NAME("Gold (troy ounce)"),       2, 1, nullptr, false, kCtMetal},
    {"XAG", u"XAG",        nullptr, nullptr,    FCT_NAME("Silver (troy ounce)"),     2, 1, nullptr, false, kCtMetal},
    {"XPT", u"XPT",        nullptr, nullptr,    FCT_NAME("Platinum (troy ounce)"),   2, 1, nullptr, false, kCtMetal},
    {"XPD", u"XPD",        nullptr, nullptr,    FCT_NAME("Palladium (troy ounce)"),  2, 1, nullptr, false, kCtMetal},

    // Crypto quote assets: 8 decimals, trailing zeros trimmed to 2 for amounts
    {"BTC",  u"\u20BF",    nullptr, nullptr,    FCT_NAME("Bitcoin"),                 8, 1, nullptr, false, kCtCrypto},
    {"ETH",  u"ETH",       nullptr, nullptr,    FCT_NAME("Ether"),                   8, 1, nullptr, false, kCtCrypto},
    {"SOL",  u"SOL",       nullptr, nullptr,    FCT_NAME("Solana"),                  8, 1, nullptr, false, kCtCrypto},
    {"BNB",  u"BNB",       nullptr, nullptr,    FCT_NAME("BNB"),                     8, 1, nullptr, false, kCtCrypto},
    {"XRP",  u"XRP",       nullptr, nullptr,    FCT_NAME("XRP"),                     8, 1, nullptr, false, kCtCrypto},
    {"ADA",  u"ADA",       nullptr, nullptr,    FCT_NAME("Cardano"),                 8, 1, nullptr, false, kCtCrypto},
    {"DOGE", u"DOGE",      nullptr, nullptr,    FCT_NAME("Dogecoin"),                8, 1, nullptr, false, kCtCrypto},
    {"LTC",  u"LTC",       nullptr, nullptr,    FCT_NAME("Litecoin"),                8, 1, nullptr, false, kCtCrypto},
    // USD stablecoins: their own currencies, converted through "<CODE>-USD"
    {"USDT",  u"USDT",     nullptr, nullptr,    FCT_NAME("Tether"),                  2, 1, nullptr, false, kCtStable},
    {"USDC",  u"USDC",     nullptr, nullptr,    FCT_NAME("USD Coin"),                2, 1, nullptr, false, kCtStable},
    {"DAI",   u"DAI",      nullptr, nullptr,    FCT_NAME("Dai"),                     2, 1, nullptr, false, kCtStable},
    {"FDUSD", u"FDUSD",    nullptr, nullptr,    FCT_NAME("First Digital USD"),       2, 1, nullptr, false, kCtStable},
    {"TUSD",  u"TUSD",     nullptr, nullptr,    FCT_NAME("TrueUSD"),                 2, 1, nullptr, false, kCtStable},
    {"BUSD",  u"BUSD",     nullptr, nullptr,    FCT_NAME("Binance USD"),             2, 1, nullptr, false, kCtStable},
};
// clang-format on

#undef FCT_NAME

constexpr int kCurrencyTableRowCount = int(sizeof(kCurrencyTableRows) / sizeof(kCurrencyTableRows[0]));

struct CurrencyTableIndex {
    QHash<QString, int> exact; // canonical spelling + aliases
    QHash<QString, int> upper; // upper-cased spelling (first one wins, so "GBP" stays pounds)
};

const CurrencyTableIndex& currency_table_index() {
    static const CurrencyTableIndex idx = [] {
        CurrencyTableIndex i;
        for (int r = 0; r < kCurrencyTableRowCount; ++r) {
            const QString code = QString::fromLatin1(kCurrencyTableRows[r].code);
            i.exact.insert(code, r);
            if (!i.upper.contains(code.toUpper()))
                i.upper.insert(code.toUpper(), r);
        }
        // Spellings some feeds send for the minor units.
        const int zac = i.exact.value(QStringLiteral("ZAc"));
        i.upper.insert(QStringLiteral("ZAC"), zac);
        i.exact.insert(QStringLiteral("ZAC"), zac);
        i.exact.insert(QStringLiteral("GBx"), i.exact.value(QStringLiteral("GBX")));
        const int usx = i.exact.value(QStringLiteral("USX"));
        i.exact.insert(QStringLiteral("USc"), usx);
        i.upper.insert(QStringLiteral("USC"), usx);
        return i;
    }();
    return idx;
}

int currency_table_row_for(const QString& code) {
    const QString c = code.trimmed();
    if (c.isEmpty())
        return -1;
    const auto& idx = currency_table_index();
    const auto exact = idx.exact.constFind(c);
    if (exact != idx.exact.constEnd())
        return exact.value();
    const auto up = idx.upper.constFind(c.toUpper());
    return up != idx.upper.constEnd() ? up.value() : -1;
}

/// Every row as a CurrencySpec without its (translated) name, built once:
/// formatting a cell copies implicitly shared strings instead of converting.
const QVector<CurrencySpec>& currency_table_specs() {
    static const QVector<CurrencySpec> specs = [] {
        QVector<CurrencySpec> v;
        v.reserve(kCurrencyTableRowCount);
        for (const CurrencyTableRow& r : kCurrencyTableRows) {
            CurrencySpec s;
            s.code = QString::fromLatin1(r.code);
            s.symbol = QString::fromUtf16(r.symbol);
            s.plain = r.plain ? QString::fromUtf16(r.plain) : s.symbol;
            s.strict = r.strict ? QString::fromUtf16(r.strict) : s.symbol;
            s.decimals = r.decimals;
            s.minor_divisor = r.minor_divisor;
            s.major = r.major ? QString::fromLatin1(r.major) : s.code;
            s.suffix = r.suffix;
            s.crypto = (r.kind & kCtCrypto) != 0;
            s.stablecoin = r.kind == kCtStable;
            s.metal = r.kind == kCtMetal;
            v.append(s);
        }
        return v;
    }();
    return specs;
}

CurrencySpec currency_table_spec(int row) {
    CurrencySpec s = currency_table_specs().at(row);
    s.name = QCoreApplication::translate("fincept::ui::formatting::Currency", kCurrencyTableRows[row].name);
    return s;
}

bool currency_table_effective(const QString& code, const FormatSettings::Snapshot& s, CurrencySpec* out,
                              bool with_name) {
    const QString c = code.trimmed();
    if (c.isEmpty())
        return false;
    const int row = currency_table_row_for(c);
    CurrencySpec spec;
    if (row >= 0)
        spec = with_name ? currency_table_spec(row) : currency_table_specs().at(row);
    const CurrencyOverride* o = nullptr;
    if (!s.overrides.isEmpty()) {
        const auto it = s.overrides.constFind(row >= 0 ? spec.code : c.toUpper());
        if (it != s.overrides.constEnd())
            o = &it.value();
    }
    if (row < 0 && !o)
        return false;
    if (row < 0) {
        const QString key = c.toUpper();
        spec.code = key;
        spec.symbol = spec.plain = spec.strict = key;
        spec.name = key;
        spec.major = key;
        spec.custom = true;
    }
    if (o) {
        if (!o->symbol.isEmpty())
            spec.symbol = spec.plain = spec.strict = o->symbol;
        if (o->decimals >= 0)
            spec.decimals = o->decimals;
        if (spec.custom && !o->name.isEmpty())
            spec.name = o->name;
    }
    if (out)
        *out = spec;
    return true;
}

/// The plain symbol `major` shows with the user's overrides applied.
QString currency_table_plain_of(const QString& major, const FormatSettings::Snapshot& s) {
    CurrencySpec spec;
    if (!currency_table_effective(major, s, &spec, false))
        return major;
    return spec.plain;
}

bool effective_currency_unnamed(const QString& code, const FormatSettings::Snapshot& s, CurrencySpec* out) {
    return currency_table_effective(code, s, out, false);
}

} // namespace currency_table_detail

bool find_currency(const QString& code, CurrencySpec* out) {
    const int row = currency_table_detail::currency_table_row_for(code);
    if (row < 0)
        return false;
    if (out)
        *out = currency_table_detail::currency_table_spec(row);
    return true;
}

bool is_known_currency(const QString& code) {
    return currency_table_detail::currency_table_row_for(code) >= 0;
}

QStringList known_currency_codes() {
    QStringList out;
    out.reserve(currency_table_detail::kCurrencyTableRowCount);
    for (const auto& r : currency_table_detail::kCurrencyTableRows)
        out.append(QString::fromLatin1(r.code));
    return out;
}

QString currency_symbol(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    if (row < 0)
        return code.trimmed().toUpper();
    return QString::fromUtf16(currency_table_detail::kCurrencyTableRows[row].symbol);
}

QString currency_symbol(const QString& code, SymbolStyle style) {
    CurrencySpec spec;
    if (!find_currency(code, &spec))
        return code.trimmed().toUpper();
    switch (style) {
        case SymbolStyle::Code:
            return spec.code;
        case SymbolStyle::Disambiguated:
            return spec.strict;
        case SymbolStyle::Symbol:
            break;
    }
    return spec.symbol;
}

int currency_decimals(const QString& code) {
    CurrencyOverride o;
    if (FormatSettings::instance().currency_override(code, &o) && o.decimals >= 0)
        return o.decimals;
    const int row = currency_table_detail::currency_table_row_for(code);
    return row < 0 ? 2 : currency_table_detail::kCurrencyTableRows[row].decimals;
}

QString currency_name(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    if (row < 0) {
        CurrencyOverride o;
        if (FormatSettings::instance().currency_override(code, &o) && !o.name.isEmpty())
            return o.name;
        return code.trimmed().toUpper();
    }
    return QCoreApplication::translate("fincept::ui::formatting::Currency",
                                       currency_table_detail::kCurrencyTableRows[row].name);
}

bool is_crypto_currency(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    return row >= 0 && (currency_table_detail::kCurrencyTableRows[row].kind & currency_table_detail::kCtCrypto) != 0;
}

bool is_stablecoin(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    return row >= 0 && currency_table_detail::kCurrencyTableRows[row].kind == currency_table_detail::kCtStable;
}

bool is_minor_unit(const QString& code) {
    return minor_unit_divisor(code) != 1;
}

int minor_unit_divisor(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    return row < 0 ? 1 : currency_table_detail::kCurrencyTableRows[row].minor_divisor;
}

QString major_currency(const QString& code) {
    const int row = currency_table_detail::currency_table_row_for(code);
    if (row < 0)
        return code.trimmed().toUpper();
    const auto& r = currency_table_detail::kCurrencyTableRows[row];
    return QString::fromLatin1(r.major ? r.major : r.code);
}

double to_major_units(double amount, const QString& code) {
    return amount / double(minor_unit_divisor(code));
}

bool effective_currency(const QString& code, const FormatSettings::Snapshot& s, CurrencySpec* out) {
    return currency_table_detail::currency_table_effective(code, s, out, true);
}

bool effective_currency(const QString& code, CurrencySpec* out) {
    return effective_currency(code, FormatSettings::instance().snapshot(), out);
}

// ── CurrencyMix ─────────────────────────────────────────────────────────────

CurrencyMix::CurrencyMix(const QStringList& codes) {
    for (const QString& c : codes)
        add(c);
}

void CurrencyMix::add(const QString& code) {
    if (code.trimmed().isEmpty())
        return;
    const QString m = major_currency(code);
    if (!majors_.contains(m))
        majors_.append(m);
}

// ── Units ───────────────────────────────────────────────────────────────────

QString currency_unit(const QString& code, const FormatSettings::Snapshot& s, const CurrencyMix* mix) {
    const QString c = code.trimmed().isEmpty() ? s.display_currency : code.trimmed();
    CurrencySpec spec;
    if (!currency_table_detail::currency_table_effective(c, s, &spec, false))
        return c.toUpper();
    switch (s.symbol_style) {
        case SymbolStyle::Code:
            return spec.code;
        case SymbolStyle::Disambiguated:
            return spec.strict;
        case SymbolStyle::Symbol:
            break;
    }
    if (spec.minor_divisor != 1) // pence, cents: their own unit
        return spec.symbol;
    const QString mine = spec.major;
    if (mix && !mix->isEmpty()) {
        for (const QString& other : mix->majors()) {
            if (other != mine && currency_table_detail::currency_table_plain_of(other, s) == spec.plain)
                return spec.strict;
        }
        return spec.plain;
    }
    // No view context: the display currency gets its plain symbol, every other
    // currency its context-free one — unless that reads like the display
    // currency's ("$" for USD when the display currency is CAD -> "US$").
    const QString home = major_currency(s.display_currency);
    if (mine == home)
        return spec.plain;
    if (spec.symbol == currency_table_detail::currency_table_plain_of(home, s))
        return spec.strict;
    return spec.symbol;
}

QString currency_unit(const QString& code) {
    return currency_unit(code, FormatSettings::instance().snapshot(), nullptr);
}

QString currency_unit(const QString& code, const CurrencyMix& mix) {
    return currency_unit(code, FormatSettings::instance().snapshot(), &mix);
}

} // namespace fincept::ui::formatting
