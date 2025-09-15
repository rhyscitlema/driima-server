importScripts("/spart/sw-caching.js");

const appAssets = [
	"/chat",
	"/favicon.ico",
	"/icons/logo-192x192.png",
	"/icons/logo-512x512.png",
	"/spart/spart.css",
	"/spart/spart.js",
	"/spart/fetch.js",
	"/spart/pages.js",
	"/spart/i18n.js",
	"/lib/bootstrap/bootstrap.min.css",
	"/lib/bootstrap/bootstrap.bundle.min.js",
	"/lib/bootstrap/bootstrap-icons.min.css",
	"/lib/bootstrap/fonts/bootstrap-icons.woff",
	"/lib/bootstrap/fonts/bootstrap-icons.woff2",
	"/lib/dompurify/purify.min.js",
	"/lib/marked/marked.min.js",
	"/lib/marked/helpers.js",
	"/lib/prism/prism.min.js",
	"/lib/prism/prism.min.css",
	"/css/login.css",
	"/css/home.css",
	"/css/chat.css",
	"/js/login.js",
	"/js/home.js",
	"/js/chat.js",
	"/js/svg.js",
	"/i18n/fr.json",
	"/i18n/en.json",
	"/manifest.json"
];

setupCaching(null, appAssets, null);
