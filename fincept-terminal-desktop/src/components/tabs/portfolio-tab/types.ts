export interface PortfolioFormState {
  name: string;
  owner: string;
  currency: string;
}

export interface AssetFormState {
  symbol: string;
  quantity: string;
  price: string;
}

export interface ModalState {
  showCreatePortfolio: boolean;
  showAddAsset: boolean;
  showSellAsset: boolean;
}

export interface PortfolioTabProps {}
