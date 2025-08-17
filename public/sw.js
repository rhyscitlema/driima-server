importScripts("/lib/spart/sw-caching.js");

const appAssets = [
	"/anonymous/chat?g=1",
	"/favicon.ico",
	"/icons/logo-192x192.png",
	"/icons/logo-512x512.png",
	"/lib/spart/spart.css",
	"/lib/spart/spart.js",
	"/lib/spart/fetch.js",
	"/lib/spart/i18n.js",
	"/lib/dompurify/purify.min.js",
	"/lib/marked/marked.min.js",
	"/lib/marked/helpers.js",
	"/lib/prism/prism.js",
	"/lib/prism/prism.css",
	"/css/login.css",
	"/css/chat.css",
	"/js/login.js",
	"/js/chat.js",
	"/js/svg.js",
	"/i18n/fr.json",
	"/i18n/en.json",
	"/manifest.json"
];

setupCaching(null, appAssets, null);
