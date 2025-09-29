(function(){
  try {
    const selectors = [
      '[id*="ad"]',
      '[class*="ad-"]',
      '[class*="ads"]',
      '[data-ad]',
      'iframe[src*="ads"]',
      'ins[class*="adsbygoogle"]'
    ];
    const style = document.createElement('style');
    style.textContent = selectors.join(',') + ' { display: none !important; visibility: hidden !important; opacity: 0 !important; height: 0 !important; }';
    document.head && document.head.appendChild(style);
  } catch(e) {
    console.warn("adblock content.js error", e);
  }
})();
