import { toast, createElement, updateElement, createSVGElement } from 'spart';
import { _fetch, sendData, showProblemDetail } from 'fetch';
import { tl } from 'i18n';
import { openPage, popPage } from 'pages';

let groupId = 0;
let joinKey = 0;

const optionsButtonSvgElem = createSVGElement(svgStrings.optionsButton);

function deletedMessage(message) {
	return !message || !message.content;
}

function onReplySnippet(event) {
	const elem = document.getElementById(event.target.dataset.messageId);
	if (elem)
		elem.scrollIntoView({ behavior: "smooth", block: "nearest" });
}

function isAI(name) {
	return name == 'AI' || name == 'IA';
}

function showOptions(event) {
	if (event.target == document.activeElement)
		setTimeout(() => event.target.blur(), 200);
}

function quitOptions(event) {
	event.target.closest("button").blur();
}

function onCopyMessage(message, e) {
	navigator.clipboard.writeText(message.content)
		.then(() => {
			quitOptions(e);
		})
		.catch(error => {
			toast("Failed to copy the message");
			console.error(error);
		});
}

function getMessageTimeSent(message) {
	const date = new Date(message.dateSent);
	const hh = String(date.getHours()).padStart(2, '0');
	const mm = String(date.getMinutes()).padStart(2, '0');
	return `${hh}:${mm}`;
}

class PageInfo {
	constructor(page) {
		this.messagesMap = {}; // messages stored by id for re-use

		// DOM elements
		this.page = page;
		this.chatContainer = null;
		this.messageInput = null;
		this.titleElem = null;

		this.parentId = null; // parent message to reply to
		this.replyText = null;
		this.replyPreview = null;

		// Fetch messages from the API
		this.fetching = false;
		this.isonline = true; // Assume online initially
		this.timerId = 0;

		this.currentSkippedMessageId = null;
		this.lastMessageDateSent = '';
		this.latestMsgDate = '';
	}

	navigateBack(event) {
		clearInterval(this.timerId);
		popPage();
	}

	// Auto-scroll the chat container to the bottom
	scrollToBottom() {
		this.chatContainer.scrollTop = this.chatContainer.scrollHeight;
	}

	// Send a new message to the API
	sendMessage() {
		const content = this.messageInput.value.trim();
		if (!content) return; // Do nothing if the message is empty

		// Build the message payload
		const payload = {
			groupId: groupId,
			joinKey: joinKey,
			parentId: this.parentId,
			content: content
		};
		sendData('/api/message/send', 'POST', payload).then(response => {
			if (response.ok) {
				// Clear input and reset reply state
				this.messageInput.value = "";
				this.cancelReply();
				this.fetchMessages(); // Fetch the new message immediately

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
	cancelReply() {
		this.parentId = null;
		this.replyPreview.classList.add("hidden");
		this.replyText.textContent = "";
	}

	getMessageInputUI() {
		return [
			{
				tag: "div", class: "reply-preview hidden",
				callback: (elem) => this.replyPreview = elem,
				content: [
					{
						tag: "span", class: "reply-text",
						events: { 'click': onReplySnippet },
						callback: (elem) => this.replyText = elem
					},
					{
						tag: "button", class: "cancel-reply", text: "x",
						events: { "click": this.cancelReply.bind(this) }
					}
				]
			},
			{
				tag: "div", class: "input-area",
				content: [
					{
						tag: "textarea", class: "message-input",
						rows: "3", placeholder: "Type a message",
						callback: (elem) => this.messageInput = elem
					},
					{
						tag: "button", class: "send-btn", text: "Send",
						events: { "click": this.sendMessage.bind(this) }
					}
				]
			}
		];
	}

	async fetchMessages() {
		if (this.fetching) return;
		this.fetching = true;

		let url = `/api/messages?groupId=${groupId}&joinKey=${joinKey}`;
		url += '&lastMessageDateSent=' + this.lastMessageDateSent;

		const response = await _fetch(url);

		if (!response.status) {
			if (this.isonline) {
				this.isonline = false;
				showProblemDetail(response);
			}
			this.fetching = false;
			return;
		}
		this.isonline = true;

		if (!response.ok) {
			showProblemDetail(response);
			this.fetching = false;
			return;
		}

		// process the successful response
		const content = await response.json();

		if (content.messages.length > 0) {
			content.messages.forEach(message => {
				// Store message
				this.messagesMap[message.id] = message;
				this.appendMessage(message);

				if (this.lastMessageDateSent < message.dateSent) {
					this.lastMessageDateSent = message.dateSent;
				}
			});
			this.scrollToBottom();
		}

		if (true)
			this.titleElem.textContent = document.title;

		this.changeSkippedMessage(content.skippedMessageId);
		this.fetching = false;
	}

	initPage() {
		const content = [
			{
				tag: "div", class: "page-header",
				content: [
					{
						tag: "button", class: "back-btn", text: "Back",
						style: "margin-right: 14px",
						events: { "click": this.navigateBack.bind(this) }
					},
					{ tag: "span", callback: (elem) => this.titleElem = elem }
				]
			},
			{
				tag: "div", class: "page-content",
				callback: (elem) => this.chatContainer = elem
			},
			{
				tag: "div", class: "chat-footer",
				content: this.getMessageInputUI()
			}
		];
		updateElement(this.page, { content });

		this.timerId = setInterval(this.fetchMessages.bind(this), 4000);
		return this.fetchMessages();
	}

	setReplySnippet(elem, messageId) {
		const message = this.messagesMap[messageId];
		if (deletedMessage(message)) {
			updateElement(elem, { text: "Reply to a deleted message" });
			return;
		}

		const maxLength = 127;
		let msg = message.content;
		if (msg.length > maxLength)
			msg = msg.slice(0, maxLength) + "...";

		const sender = message.senderName;
		const content = [
			{ text: sender, tag: isAI(sender) ? 'span' : null },
			{ text: ": " + msg }
		];
		updateElement(elem, { content });
		elem.dataset.messageId = messageId;
	}

	onReplyButton(message) {
		this.parentId = message.id;
		this.setReplySnippet(this.replyText, message.id);
		this.replyPreview.classList.remove("hidden");
		const mi = this.messageInput;

		if (!mi.value) {
			const sender = message.senderName;
			if (isAI(sender))
				mi.value = '@' + tl(sender) + ' ';
		}
		mi.focus();
	}

	onDeleteMessage(message, e) {
		quitOptions(e);
		if (confirm("Please confirm you want to delete")) {
			const url = "/api/message/delete?id=" + message.id;
			_fetch(url, { method: "DELETE" }).then((response) => {
				if (response.ok) {
					const elem = document.getElementById(message.id);
					elem.remove();
					message.content = null;
				}
				else showProblemDetail(response);
			});
		}
	}

	changeSkippedMessage(messageId) {
		let id = this.currentSkippedMessageId;
		if (id == messageId)
			return;

		if (id) {
			const elem = document.getElementById(id);
			if (elem)
				elem.classList.remove("ai-skipped");
		}

		id = messageId;
		this.currentSkippedMessageId = id;

		if (id) {
			const elem = document.getElementById(id);
			if (elem)
				elem.classList.add("ai-skipped");
		}
	}

	onHideFromAI(message, e) {
		quitOptions(e);

		if (!localStorage.firstHideFromAI) {
			localStorage.firstHideFromAI = Date.now();
			alert(tl("All prior messages will be skipped"));
		}

		const url = "/api/message/hide-from-ai?id=" + message.id;
		_fetch(url, { method: "PATCH" }).then((response) => {
			if (response.ok)
				this.changeSkippedMessage(message.id);
			else showProblemDetail(response);
		});
	}

	appendDateSeparator(message) {
		const dateStr = new Date(message.dateSent).toDateString();
		if (this.latestMsgDate == dateStr)
			return;
		this.latestMsgDate = dateStr;
		const info = {
			tag: 'p', class: "date-separator",
			content: [{ tag: 'span', content: [{ text: this.latestMsgDate }] }]
		};
		this.chatContainer.appendChild(createElement(info));
	}

	sentOrCausedByMe(message) {
		while (true) {
			if (!message)
				break;

			if (message.sentByMe)
				return true;

			if (!isAI(message.senderName) || !message.parentId)
				break;

			message = this.messagesMap[message.parentId];
		}
		return false;
	}

	createMessageOptionsButton(message) {
		const options = [
			{ text: "Copy", events: { 'click': (e) => onCopyMessage(message, e) } },
			{ text: "Reply", events: { 'click': () => this.onReplyButton(message) } },
		];

		if (this.sentOrCausedByMe(message)) {
			options.push({
				text: "Delete", class: "delete-msg",
				events: { 'click': (e) => this.onDeleteMessage(message, e) }
			});
			options.push({
				text: "Hide from AI", class: "hide-from-ai",
				events: { 'click': (e) => this.onHideFromAI(message, e) }
			});
		}

		return createElement({
			tag: "button",
			class: "options-button",
			events: { "mousedown": showOptions },
			content: [
				{ element: optionsButtonSvgElem.cloneNode(true) },
				{ tag: "ul", class: "options-list", content: options }
			]
		});
	}

	// Append a message to the chat container
	appendMessage(message) {
		if (deletedMessage(message))
			return;

		this.appendDateSeparator(message);

		const content = [
			{ element: this.createMessageOptionsButton(message) },
			{ tag: 'div', class: 'sender-name', text: message.senderName },
			(
				message.parentId && {
					tag: 'div', class: 'reply-snippet',
					events: { 'click': onReplySnippet },
					callback: (elem) => this.setReplySnippet(elem, message.parentId),
				}
			),
			{
				tag: 'div', class: 'content',
				callback: (elem) => convertMarkdownText(elem, message.content)
			},
			{
				tag: 'div', class: 'message-footer',
				content: [
					{
						tag: 'button', class: 'reply-btn', text: 'Reply',
						events: { 'click': () => this.onReplyButton(message) }
					},
					{
						tag: 'span', class: 'date-sent',
						content: [{ text: getMessageTimeSent(message) }]
					}
				]
			}
		];

		this.chatContainer.appendChild(createElement({
			tag: 'div',
			id: message.id,
			class: 'message ' + (message.sentByMe ? "sent" : "received"),
			content
		}));
	}
}

/**
 * Open the Chat page
 * @param {URLSearchParams} params
 * @returns {Promise<void>}
 */
export default function openChatPage(params) {
	const page = openPage("chat");
	page.classList.add("flex-column");

	if (page.childElementCount) {
		console.warn("How did we get here?");
		return;
	}

	if (params) {
		groupId = +(params.get('g') || "0");
		joinKey = +(params.get('k') || "0");
	}

	const x = new PageInfo(page);
	return x.initPage();
}
