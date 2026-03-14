"""
Form 4 Insider Transaction Extraction
======================================

Extract insider trading data from Form 4 filings including:
- Buy/sell transactions
- Officer/director information
- Transaction prices and shares
- Ownership before/after
- Derivative vs non-derivative securities
"""

from typing import Dict, Any, Optional
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


def get_insider_transactions(ticker: str, limit: int = 25) -> Dict[str, Any]:
    """
    Get insider transactions (Form 4 filings)

    Args:
        ticker: Stock ticker symbol
        limit: Number of transactions to retrieve

    Returns:
        List of insider transactions with basic info
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="4")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": [],
                "count": 0
            }

        transactions = []
        count = 0

        for filing in filings:
            if count >= limit:
                break

            try:
                form4 = filing.obj()

                if form4:
                    transactions.append({
                        "filing_date": str(filing.filing_date),
                        "accession_number": filing.accession_number,
                        "owner": form4.owner.name if hasattr(form4, 'owner') and hasattr(form4.owner, 'name') else None,
                        "is_director": form4.owner.is_director if hasattr(form4, 'owner') and hasattr(form4.owner, 'is_director') else None,
                        "is_officer": form4.owner.is_officer if hasattr(form4, 'owner') and hasattr(form4.owner, 'is_officer') else None,
                    })
                    count += 1
            except:
                # Skip if can't parse
                pass

        return {
            "success": True,
            "data": transactions,
            "count": len(transactions)
        }
    except Exception as e:
        return {"error": EdgarError("get_insider_transactions", str(e), traceback.format_exc()).to_dict()}


def get_insider_transactions_detailed(ticker: str, limit: int = 25) -> Dict[str, Any]:
    """
    Get detailed insider transactions with full transaction info

    Args:
        ticker: Stock ticker symbol
        limit: Number of transactions to retrieve

    Returns:
        Detailed transaction data including prices, shares, ownership
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="4")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": [],
                "count": 0
            }

        transactions = []
        count = 0

        for filing in filings:
            if count >= limit:
                break

            try:
                form4 = filing.obj()

                if not form4:
                    continue

                # Extract owner info
                owner_info = {
                    "name": form4.owner.name if hasattr(form4, 'owner') and hasattr(form4.owner, 'name') else None,
                    "is_director": form4.owner.is_director if hasattr(form4, 'owner') and hasattr(form4.owner, 'is_director') else False,
                    "is_officer": form4.owner.is_officer if hasattr(form4, 'owner') and hasattr(form4.owner, 'is_officer') else False,
                    "is_ten_percent_owner": form4.owner.is_ten_percent_owner if hasattr(form4, 'owner') and hasattr(form4.owner, 'is_ten_percent_owner') else False,
                    "officer_title": form4.owner.officer_title if hasattr(form4, 'owner') and hasattr(form4.owner, 'officer_title') else None,
                }

                # Extract transactions
                txn_data = []
                if hasattr(form4, 'transactions'):
                    for txn in form4.transactions:
                        txn_info = {
                            "transaction_date": str(txn.transaction_date) if hasattr(txn, 'transaction_date') else None,
                            "transaction_code": txn.transaction_code if hasattr(txn, 'transaction_code') else None,
                            "shares": float(txn.shares) if hasattr(txn, 'shares') and txn.shares else None,
                            "price_per_share": float(txn.price_per_share) if hasattr(txn, 'price_per_share') and txn.price_per_share else None,
                            "transaction_type": "Buy" if hasattr(txn, 'transaction_code') and txn.transaction_code in ['P', 'M'] else "Sell" if hasattr(txn, 'transaction_code') and txn.transaction_code in ['S', 'F'] else "Other",
                            "ownership_form": txn.ownership_form if hasattr(txn, 'ownership_form') else None,
                            "shares_owned_after": float(txn.shares_owned_after) if hasattr(txn, 'shares_owned_after') and txn.shares_owned_after else None,
                        }
                        txn_data.append(txn_info)

                transactions.append({
                    "filing_date": str(filing.filing_date),
                    "accession_number": filing.accession_number,
                    "owner": owner_info,
                    "transactions": txn_data,
                    "transaction_count": len(txn_data)
                })

                count += 1

            except Exception as e:
                # Skip problematic filings
                continue

        return {
            "success": True,
            "data": transactions,
            "count": len(transactions)
        }
    except Exception as e:
        return {"error": EdgarError("get_insider_transactions_detailed", str(e), traceback.format_exc()).to_dict()}


def get_insider_summary(ticker: str, limit: int = 50) -> Dict[str, Any]:
    """
    Get summary of insider activity (buy vs sell)

    Args:
        ticker: Stock ticker symbol
        limit: Number of filings to analyze

    Returns:
        Summary statistics of insider activity
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="4")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": {
                    "total_transactions": 0,
                    "buys": 0,
                    "sells": 0,
                    "buy_volume": 0,
                    "sell_volume": 0
                }
            }

        buys = 0
        sells = 0
        buy_volume = 0
        sell_volume = 0
        count = 0

        for filing in filings:
            if count >= limit:
                break

            try:
                form4 = filing.obj()
                if not form4 or not hasattr(form4, 'transactions'):
                    continue

                for txn in form4.transactions:
                    if hasattr(txn, 'transaction_code') and hasattr(txn, 'shares'):
                        code = txn.transaction_code
                        shares = float(txn.shares) if txn.shares else 0

                        if code in ['P', 'M']:  # Purchase
                            buys += 1
                            buy_volume += shares
                        elif code in ['S', 'F']:  # Sale
                            sells += 1
                            sell_volume += shares

                count += 1
            except:
                continue

        return {
            "success": True,
            "data": {
                "total_transactions": buys + sells,
                "buys": buys,
                "sells": sells,
                "buy_volume": buy_volume,
                "sell_volume": sell_volume,
                "net_volume": buy_volume - sell_volume
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_insider_summary", str(e), traceback.format_exc()).to_dict()}
