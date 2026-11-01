import tls from 'node:tls';

export function mergeCertificateStores(...stores) {
  return [...new Set(stores.flat())];
}

export function useSystemCertificates() {
  if (
    typeof tls.getCACertificates !== 'function' ||
    typeof tls.setDefaultCACertificates !== 'function'
  ) {
    return false;
  }
  try {
    tls.setDefaultCACertificates(
      mergeCertificateStores(
        tls.getCACertificates('default'),
        tls.getCACertificates('system')
      )
    );
    return true;
  } catch {
    return false;
  }
}
