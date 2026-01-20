// Support API Service - Feedback, Contact, Newsletter endpoints

const API_BASE_URL = 'https://finceptbackend.share.zrok.io';

// Types
export interface ContactFormData {
  name: string;
  email: string;
  subject: string;
  message: string;
}

export interface FeedbackFormData {
  name?: string;
  email?: string;
  rating: number;
  feedback_text: string;
  category?: string;
}

export interface NewsletterSubscribeData {
  email: string;
  source?: string;
}

export interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
}

// API Functions

/**
 * Submit contact form (public endpoint)
 */
export async function submitContactForm(data: ContactFormData): Promise<ApiResponse> {
  try {
    const response = await fetch(`${API_BASE_URL}/support/contact`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(data),
    });

    const result = await response.json();

    if (!response.ok) {
      return {
        success: false,
        error: result.detail || 'Failed to submit contact form',
      };
    }

    return {
      success: true,
      data: result,
      message: 'Contact form submitted successfully',
    };
  } catch (error) {
    console.error('Contact form submission error:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
    };
  }
}

/**
 * Submit feedback (public endpoint)
 */
export async function submitFeedback(data: FeedbackFormData): Promise<ApiResponse> {
  try {
    const response = await fetch(`${API_BASE_URL}/support/feedback`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(data),
    });

    const result = await response.json();

    if (!response.ok) {
      return {
        success: false,
        error: result.detail || 'Failed to submit feedback',
      };
    }

    return {
      success: true,
      data: result,
      message: 'Feedback submitted successfully',
    };
  } catch (error) {
    console.error('Feedback submission error:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
    };
  }
}

/**
 * Subscribe to newsletter (public endpoint)
 */
export async function subscribeToNewsletter(data: NewsletterSubscribeData): Promise<ApiResponse> {
  try {
    const response = await fetch(`${API_BASE_URL}/support/newsletter/subscribe`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(data),
    });

    const result = await response.json();

    if (!response.ok) {
      return {
        success: false,
        error: result.detail || 'Failed to subscribe to newsletter',
      };
    }

    return {
      success: true,
      data: result,
      message: 'Successfully subscribed to newsletter',
    };
  } catch (error) {
    console.error('Newsletter subscription error:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
    };
  }
}

/**
 * Get support categories (public endpoint)
 */
export async function getSupportCategories(): Promise<ApiResponse<string[]>> {
  try {
    const response = await fetch(`${API_BASE_URL}/support/categories`, {
      method: 'GET',
      headers: {
        'Content-Type': 'application/json',
      },
    });

    const result = await response.json();

    if (!response.ok) {
      return {
        success: false,
        error: result.detail || 'Failed to fetch categories',
      };
    }

    return {
      success: true,
      data: result,
    };
  } catch (error) {
    console.error('Get categories error:', error);
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error',
    };
  }
}
