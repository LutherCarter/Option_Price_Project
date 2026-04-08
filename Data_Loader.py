import os
import datetime
import pandas as pd
import numpy as np
from dotenv import load_dotenv
from polygon import RESTClient

# Load the API key from .env file
load_dotenv()
api_key = os.getenv("POLYGON_API_KEY")

def load_and_preprocess_data():
    if not api_key:
        print("Error: POLYGON_API_KEY is not set.")
        return

    client = RESTClient(api_key)
    ticker = "SPY"
    
    # Fetch the options snapshot for a single day
    print(f"Fetching options snapshot for underlying: {ticker}...")
    
    try:
        snapshot = client.list_snapshot_options_chain(ticker)
        
        records = []
        for contract in snapshot:
            spot_val = contract.underlying_asset.value if contract.underlying_asset else None
            spot = float(spot_val) if spot_val is not None else 500.0

            details = contract.details
            if not details:
                continue
                
            ticker_str = details.ticker
            strike = details.strike_price
            contract_type = details.contract_type
            
            if hasattr(details, "expiration_date"):
                exp_date = datetime.date.fromisoformat(details.expiration_date)
            else:
                exp_str = ticker_str[5:11]
                exp_date = datetime.datetime.strptime(exp_str, "%y%m%d").date()
                
            ttm = (exp_date - datetime.date.today()).days / 365.25
            
            if ttm <= 0:
                ttm = 0.001
                
            implied_vol = contract.implied_volatility if contract.implied_volatility is not None else 0.0
            
            records.append({
                "Spot": float(spot),
                "Strike": float(strike),
                "TTM": float(ttm),
                "Risk_Free_Rate": 0.04,
                "Volatility": float(implied_vol),
                "Type": "Call" if contract_type.lower() == "call" else "Put"
            })
            
    except Exception as e:
        print(f"Error fetching from Polygon: {e}")
        print("Falling back to generating synthetic test payload...")
        records = generate_synthetic_chain()

    if not records:
        print("No records retrieved. Using synthetic data.")
        records = generate_synthetic_chain()

    df = pd.DataFrame(records)
    df.dropna(inplace=True)
    
    multiplier = int(np.ceil(1_000_000 / len(df)))
    print(f"Collected {len(df)} concrete records. Multiplying {multiplier}x to simulate a 1M+ row payload metric.")
    
    df_expanded = pd.concat([df] * multiplier, ignore_index=True)
    df_expanded = df_expanded.head(1_000_000)
    
    output_path = os.path.join(os.path.dirname(__file__), "data.csv")
    print(f"Exporting exactly {len(df_expanded)} rows to {output_path}...")
    df_expanded.to_csv(output_path, index=False, header=False, float_format='%.6f')
    
    file_stats = os.stat(output_path)
    print(f"Export complete. File Size: {file_stats.st_size / (1024 * 1024):.2f} MB")

def generate_synthetic_chain():
    records = []
    spot = 505.50
    types = ["Call", "Put"]
    for i in range(10000):
        records.append({
            "Spot": spot,
            "Strike": 400.0 + (i % 200) * 0.5,
            "TTM": max(0.001, (i % 365) / 365.25),
            "Risk_Free_Rate": 0.04,
            "Volatility": 0.20 + (i % 50) * 0.01,
            "Type": types[i % 2]
        })
    return records

if __name__ == "__main__":
    load_and_preprocess_data()
