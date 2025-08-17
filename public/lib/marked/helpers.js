/*
	Helper functions useful for converting markdown content found inside a page.
*/

/**
 * Set the inner HTML of a div element with converted markdown text.
 * The HTML is first sanitized, using DOMPurify.
 * Supports presence of source code, using Prism.
 *
 * @param {HTMLDivElement} divElem
 * @param {string} text
 */
function convertMarkdownText(divElem, text) {
	// Convert the markdown to HTML using marked.js
	let html = marked.parse(text);

	// Sanitize the HTML output using DOMPurify for security
	html = DOMPurify.sanitize(html);

	// Render the safe HTML into your output element
	divElem.innerHTML = html;

	// Locate code blocks
	const codeBlocks = divElem.querySelectorAll('code[class^="language-"]');

	// Convert the code to HTML using Prism.js
	codeBlocks.forEach(codeBlock => Prism.highlightElement(codeBlock));
}

function convertMarkdownTexts(parent) {
	const className = "markdown-text";
	const elems = parent.getElementsByClassName(className);
	for (let i = 0; i < elems.length; i++) {
		const elem = elems[i];
		convertMarkdownText(elem, elem.textContent);
		elem.classList.remove(className);
	}
}

function convertMarkdownURL(elem) {
	const url = elem.dataset.url;
	if (url)
		return fetch(url).then(response => response.text())
			.then(text => convertMarkdownText(elem, text));
}

function convertMarkdownURLs(parent) {
	const className = "markdown-url";
	const elems = parent.getElementsByClassName(className);
	for (let i = 0; i < elems.length; i++) {
		const elem = elems[i];
		convertMarkdownURL(elem);
		elem.classList.remove(className);
	}
}
