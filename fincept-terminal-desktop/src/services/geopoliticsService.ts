import { invoke } from '@tauri-apps/api/core';

export interface GeopoliticsData {
  tariffData: any[];
  qrNotifications: any[];
  epingNotifications: any[];
  qrRestrictions: any[];
  qrProducts: any[];
  tradeFlowData: any[];
  tfadData: any;
  tradeIndicators: any[];
  reporters: any[];
  partners: any[];
}

class GeopoliticsService {
  private wtoApiKey = 'd7a9d5f751404d189deb99efcb855d12';

  async getWTOQRNotifications(): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'qr_notifications',
        args: [this.wtoApiKey]
      });
      const parsed = typeof result === 'string' ? JSON.parse(result) : result;
      console.log('QR raw:', parsed);
      return parsed;
    } catch (error) {
      console.error('Failed to fetch WTO QR notifications:', error);
      return null;
    }
  }

  async getWTOEPingNotifications(pageSize: number = 20): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'eping_search',
        args: [`--page_size=${pageSize}`, this.wtoApiKey]
      });
      const parsed = typeof result === 'string' ? JSON.parse(result) : result;
      console.log('ePing raw:', parsed);
      return parsed;
    } catch (error) {
      console.error('Failed to fetch WTO ePing notifications:', error);
      return null;
    }
  }

  async getWTOTimeseriesData(indicator: string, reporters: string, periods: string): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'timeseries_data',
        args: [`--i=${indicator}`, `--reporters=${reporters}`, `--periods=${periods}`, this.wtoApiKey]
      });
      const parsed = typeof result === 'string' ? JSON.parse(result) : result;
      console.log('Tariff raw:', parsed);
      return parsed;
    } catch (error) {
      console.error('Failed to fetch WTO timeseries data:', error);
      return null;
    }
  }

  async getWTOQRList(countryCode: string): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'qr_list',
        args: [`--reporter_member_code=${countryCode}`, this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO QR list:', error);
      return null;
    }
  }

  async getWTOQRProducts(hsVersion: string = '2017'): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'qr_products',
        args: [`--hs_version=${hsVersion}`, this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO QR products:', error);
      return null;
    }
  }

  async getWTOReporters(): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'timeseries_reporters',
        args: [this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO reporters:', error);
      return null;
    }
  }

  async getWTOPartners(): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'timeseries_partners',
        args: [this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO partners:', error);
      return null;
    }
  }

  async getWTOTFAD(countries: string): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'tfad_data',
        args: [`--countries=${countries}`, this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO TFAD data:', error);
      return null;
    }
  }

  async getWTOTradeFlow(indicator: string, reporters: string, partners: string, periods: string): Promise<any> {
    try {
      const result = await invoke('execute_wto_command', {
        command: 'timeseries_data',
        args: [`--i=${indicator}`, `--reporters=${reporters}`, `--partners=${partners}`, `--periods=${periods}`, this.wtoApiKey]
      });
      return typeof result === 'string' ? JSON.parse(result) : result;
    } catch (error) {
      console.error('Failed to fetch WTO trade flow data:', error);
      return null;
    }
  }

  async getComprehensiveGeopoliticsData(countryCode: string = '840'): Promise<GeopoliticsData> {
    const currentYear = new Date().getFullYear();

    const [
      qrData,
      epingData,
      tariffData,
      qrListData,
      qrProductsData,
      tfadData,
      tradeFlowData,
      reportersData,
      partnersData
    ] = await Promise.all([
      this.getWTOQRNotifications(),
      this.getWTOEPingNotifications(50),
      this.getWTOTimeseriesData('TP_A_0010', countryCode, `${currentYear-10}-${currentYear-1}`),
      this.getWTOQRList(countryCode),
      this.getWTOQRProducts('2017'),
      this.getWTOTFAD(countryCode),
      this.getWTOTradeFlow('ITS_MTV_AM', countryCode, 'all', `${currentYear-5}-${currentYear-1}`), // Merchandise trade value
      this.getWTOReporters(),
      this.getWTOPartners()
    ]);

    // Parse QR notifications - response has data.data[]
    const qrNotifications = qrData?.data?.data || [];

    // Parse ePing notifications - response has data.items[]
    const epingNotifications = epingData?.data?.items || [];

    // Parse tariff data - response has data.Dataset[]
    const tariffDataArray = tariffData?.data?.Dataset || [];

    // Parse QR restrictions list
    const qrRestrictions = qrListData?.data?.data || [];

    // Parse QR products
    const qrProducts = qrProductsData?.data?.data || [];

    // Parse TFAD data
    const tfad = tfadData?.data || null;

    // Parse trade flow data
    const tradeFlow = tradeFlowData?.data?.Dataset || [];

    // Parse reporters
    const reporters = reportersData?.data || [];

    // Parse partners
    const partners = partnersData?.data || [];

    return {
      tariffData: tariffDataArray,
      qrNotifications,
      epingNotifications,
      qrRestrictions,
      qrProducts,
      tradeFlowData: tradeFlow,
      tfadData: tfad,
      tradeIndicators: [], // Will be populated separately if needed
      reporters,
      partners
    };
  }
}

export const geopoliticsService = new GeopoliticsService();
