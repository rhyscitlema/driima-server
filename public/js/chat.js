import { toast, createElement, updateElement, createSVGElement } from 'spart';
import { _fetch, apiFetch, showProblemDetail } from 'fetch';
import { tl, currentLanguage, changeLanguage } from 'i18n';

const urlParams = new URLSearchParams(window.location.search);
const groupId = +(urlParams.get('g') || "0");
const joinKey = +(urlParams.get('k') || "0");

// Global variables
let lastMessageDateSent = '';
let parentId = null;
const messagesMap = {}; // messages stored by id for re-use

// DOM elements
let chatContainer = null;
let messageInput = null;
let sendBtn = null;
let replyPreview = null;
let replyText = null;
let cancelReplyBtn = null;

export function initializeElements() {
	if (chatContainer)
		return; // Already initialized

	chatContainer = document.getElementById("chat-container");
	messageInput = document.getElementById("message-input");
	sendBtn = document.getElementById("send-btn");
	replyPreview = document.getElementById("reply-preview");
	replyText = document.getElementById("reply-text");
	cancelReplyBtn = document.getElementById("cancel-reply");

	// Event listeners
	sendBtn.addEventListener("click", sendMessage);
	cancelReplyBtn.addEventListener("click", cancelReply);

	updateElement(messageInput, { placeholder: "Type a message" });
	updateElement(sendBtn, { text: "Send" });

	const language = document.getElementById("language");
	language.value = currentLanguage;
	language.addEventListener('change', (e) => changeLanguage(e.target.value));
}

// Fetch messages from the API
let fetching = false;
let isonline = true; // Assume online initially

export async function fetchMessages() {
	if (fetching) return;
	fetching = true;

	let url = `/api/message/many?groupId=${groupId}&joinKey=${joinKey}`;
	url += '&lastMessageDateSent=' + lastMessageDateSent;

	const response = await _fetch(url);

	if (!response.status) {
		if (isonline) {
			isonline = false;
			showProblemDetail(response);
		}
		fetching = false;
		return;
	}
	isonline = true;

	if (!response.ok) {
		showProblemDetail(response);
		fetching = false;
		return;
	}

	// process the successful response
	const content = await response.json();

	if (content.messages.length > 0) {
		content.messages.forEach(message => {
			// Store message
			messagesMap[message.id] = message;
			appendMessage(message);

			if (message.dateSent > lastMessageDateSent) {
				lastMessageDateSent = message.dateSent;
			}
		});
		scrollToBottom();
	}

	changeSkippedMessage(content.skippedMessageId);
	fetching = false;
}

/**
 * Set the inner HTML of a div element with sanitized markdown content
 * @param {HTMLDivElement} divElement
 * @param {object} markdownResponse
 */
function setMessageContent(divElement, message) {

	// Convert the markdown to HTML using marked.js
	let html = marked.parse(message.content);

	// Sanitize the HTML output using DOMPurify for security
	html = DOMPurify.sanitize(html);

	// Render the safe HTML into your chat output element
	divElement.innerHTML = html;

	const codeBlocks = divElement.querySelectorAll('code[class^="language-"]');
	codeBlocks.forEach(codeBlock => Prism.highlightElement(codeBlock));
}

function deletedMessage(message) {
	return !message || !message.content;
}

function getSnippet(messageId) {
	const message = messagesMap[messageId];
	if (deletedMessage(message))
		return "Replying to a deleted message";

	const maxLength = 127;
	const content = message.content.length > maxLength ? message.content.slice(0, maxLength) + "..." : message.content;
	return message.senderName + ": " + content;
}

function onReplySnippet(message) {
	const elem = document.getElementById(message.parentId);
	if (elem)
		elem.scrollIntoView({ behavior: "smooth", block: "nearest" });
}

function onReplyButton(message) {
	parentId = message.id;
	replyText.textContent = getSnippet(message.id);
	replyPreview.classList.remove("hidden");

	if (!messageInput.value) {
		const sender = message.senderName;
		if (sender == 'AI' || sender == 'IA')
			messageInput.value = '@' + sender + ' ';
	}
	messageInput.focus();
}

function quit_options(event) {
	event.target.closest("button").blur();
}

function onDeleteMessage(message, e) {
	quit_options(e);
	if (confirm("Please confirm you want to delete")) {
		const url = "/api/message/delete?id=" + message.id;
		apiFetch(url, "DELETE").then((response) => {
			if (response.ok) {
				const elem = document.getElementById(message.id);
				elem.remove();
				message.content = null;
			}
			else showProblemDetail(response);
		});
	}
}

let currentSkippedMessageId = null;

function changeSkippedMessage(messageId) {
	let id = currentSkippedMessageId;
	if (id == messageId)
		return;

	if (id) {
		const elem = document.getElementById(id);
		if (elem)
			elem.classList.remove("ai-skipped");
	}

	id = messageId;
	currentSkippedMessageId = id;
	if (id) {
		const elem = document.getElementById(id);
		if (elem)
			elem.classList.add("ai-skipped");
	}
}

function onHideFromAI(message, e) {
	quit_options(e);

	if (!localStorage.firstHideFromAI) {
		localStorage.firstHideFromAI = Date.now();
		alert(tl("All prior messages will be skipped"));
	}

	const url = "/api/message/hide-from-ai?id=" + message.id;
	apiFetch(url, "PATCH").then((response) => {
		if (response.ok)
			changeSkippedMessage(message.id);
		else showProblemDetail(response);
	});
}

function onCopyMessage(message, e) {
	navigator.clipboard.writeText(message.content)
		.then(() => {
			quit_options(e);
		})
		.catch(error => {
			toast("Failed to copy the message");
			console.error(error);
		});
}

const optionsButtonSvgElem = createSVGElement(svgStrings.optionsButton);

function sentOrCausedByMe(message) {
	while (true) {
		if (!message)
			break;

		if (message.sentByMe)
			return true;

		if (message.senderName != "AI" || !message.parentId)
			break;

		message = messagesMap[message.parentId];
	}
	return false;
}

function getMessageTimeSent(message) {
	const date = new Date(message.dateSent);
	const hh = String(date.getHours()).padStart(2, '0');
	const mm = String(date.getMinutes()).padStart(2, '0');
	return `${hh}:${mm}`;
}

let latest = '';
function appendDateSeparator(message) {
	const dateStr = new Date(message.dateSent).toDateString();
	if (latest != dateStr) {
		latest = dateStr;
		chatContainer.appendChild(createElement({
			tag: 'p', class: "date-separator",
			content: [{ tag: 'span', text: latest }]
		}));
	}
}

function createMessageOptionsButton(message) {
	const options = [
		{ text: "Copy", events: { 'click': (e) => onCopyMessage(message, e) } },
		{ text: "Reply", events: { 'click': () => onReplyButton(message) } },
	];

	if (sentOrCausedByMe(message)) {
		options.push({ text: "Delete", events: { 'click': (e) => onDeleteMessage(message, e) }, class: "delete-msg" });
		options.push({ text: "Hide from AI", events: { 'click': (e) => onHideFromAI(message, e) }, class: "hide-from-ai" });
	}

	return createElement({
		tag: "button",
		class: "options-button",
		events: {
			"mousedown": function (e) {
				if (e.target == document.activeElement)
					setTimeout(() => e.target.blur(), 200);
			}
		},
		content: [
			{ element: optionsButtonSvgElem.cloneNode(true) },
			{ tag: "ul", class: "options-list", content: options }
		]
	});
}

// Append a message to the chat container
function appendMessage(message) {
	if (deletedMessage(message))
		return;

	appendDateSeparator(message);

	chatContainer.appendChild(createElement({
		tag: 'div',
		id: message.id,
		class: 'message ' + (message.sentByMe ? "sent" : "received"),
		content: [
			{ element: createMessageOptionsButton(message) },
			{ tag: 'div', class: 'sender-name', text: message.senderName },
			(
				message.parentId && {
					tag: 'div', class: 'reply-snippet',
					text: getSnippet(message.parentId),
					events: { 'click': () => onReplySnippet(message) }
				}
			),
			{
				tag: 'div', class: 'content',
				callback: (elem) => setMessageContent(elem, message)
			},
			{
				tag: 'div', class: 'message-footer',
				content: [
					{
						tag: 'button', class: 'reply-btn', text: 'Reply',
						events: { 'click': () => onReplyButton(message) }
					},
					{
						tag: 'span', class: 'date-sent',
						text: getMessageTimeSent(message)
					}
				]
			}
		]
	}));
}

// Auto-scroll the chat container to the bottom
function scrollToBottom() {
	chatContainer.scrollTop = chatContainer.scrollHeight;
}

// Send a new message to the API
function sendMessage() {
	const content = messageInput.value.trim();
	if (!content) return; // Do nothing if the message is empty
	// Build the message payload
	const payload = {
		groupId: groupId,
		joinKey: joinKey,
		parentId: parentId,
		content: content
	};
	apiFetch('/api/message/send', 'POST', payload).then(response => {
		if (response.ok) {
			// Clear input and reset reply state
			messageInput.value = "";
			cancelReply();
			fetchMessages(); // Fetch the new message immediately

			return response.json().then(info => {
				if (info.ai_is_busy) {
					toast("AI is busy responding, please wait");
				}
			});
		}
		else showProblemDetail(response);
	});
}

// Cancel the current reply
function cancelReply() {
	parentId = null;
	replyPreview.classList.add("hidden");
	replyText.textContent = "";
}
