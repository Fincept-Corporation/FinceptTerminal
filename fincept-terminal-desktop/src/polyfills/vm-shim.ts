// Browser polyfill for node vm module
export function runInNewContext(code: string, sandbox?: any): any {
  // In browser, we can't truly create a VM context
  // This is a minimal polyfill that may not work for all cases
  console.warn('vm.runInNewContext is not fully supported in browser');
  try {
    const func = new Function(...Object.keys(sandbox || {}), code);
    return func(...Object.values(sandbox || {}));
  } catch (error) {
    console.error('Error running code in new context:', error);
    return undefined;
  }
}

export function runInThisContext(code: string): any {
  try {
    return eval(code);
  } catch (error) {
    console.error('Error running code in this context:', error);
    return undefined;
  }
}

export default { runInNewContext, runInThisContext };
