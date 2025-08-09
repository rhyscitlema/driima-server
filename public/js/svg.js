
let x = window.svgStrings;
if (!x) {
	x = {};
	window.svgStrings = x;
};

x.optionsButton = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" focusable="false">
	<path d="M7.41 8.59 12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z" />
</svg>`;

x.appLogo = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">
	<!-- two matching half-ellipses (arcs) -->
	<path
		d="M15,85 A90,65 0 0 1 185,85"
		fill="none"
		stroke="#000"
		stroke-width="14"
		stroke-linecap="round"
	/>
	<path
		d="M35,103 A70,55 0 0 1 165,103"
		fill="none"
		stroke="#000"
		stroke-width="14"
		stroke-linecap="round"
	/>

	<ellipse cx="100" cy="120" rx="50" ry="30" fill="#000" />

	<circle cx="80"  cy="120" r="5" fill="white" />
	<circle cx="100" cy="120" r="5" fill="white" />
	<circle cx="120" cy="120" r="5" fill="white" />

	<polygon points="83,140 117,140 100,175" fill="#000" />
</svg>`;

