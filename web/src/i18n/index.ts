import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';

import enTranslation from './locales/en.json';
import zhTranslation from './locales/zh.json';
import zhTwTranslation from './locales/zh-TW.json';

i18n
  .use(initReactI18next)
  .init({
    resources: {
      en: { ...enTranslation },
      zh: { ...zhTranslation },
      'zh-TW': { ...zhTwTranslation },
    },
    lng: 'zh', // default language
    fallbackLng: 'en',
    interpolation: {
      escapeValue: false, // react already safes from xss
    },
  });

export default i18n;
