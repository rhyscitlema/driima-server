import store from 'store';
import openChatPage from 'chat';
import { openPage } from 'pages';
import { currentLanguage, changeLanguage } from 'i18n';
import { toast, createElement, updateElement } from 'spart';
import { _fetch, sendData, showProblemDetail } from 'fetch';

function roomSelected(e) {
	const params = new URLSearchParams();
	params.set("r", e.currentTarget.dataset.id);
	openChatPage(params);
};

function getRoomUI(info) {
	const name = info.groupName + (info.roomName ? (": " + info.roomName) : "");

	const latest =
		!info.latestDateSent ? { tag: "i", text: "(no message)" }:
		!info.latestMessage ? { tag: "i", text: "(deleted message)" } :
		{ text: info.latestMessage };

	const avatar = info.logo ?
		{ tag: "img", class: "avatar", alt: "profile", src: info.logo }
		: {
			tag: "div", class: "avatar",
			content: [{ tag: "i", class: "bi bi-people-fill", style: "font-size: 2em" }]
		};

	return [
		avatar,
		{
			tag: "div", class: "chat-room-info",
			content: [
				{
					tag: "div", class: "info-body",
					content: [
						{ tag: "h5", class: "room-name", html: name },
						{ tag: "p", class: "latest-message", content: [latest] }
					]
				}
			]
		}
	];
}

let page_content = null;

function setRooms(rooms) {
	if (rooms == null) {
		return fetchRooms();
	}
	let content;

	if (rooms.length == 0) {
		content = [{
			tag: "div",
			class: "chat-room",
			"data-id": 1,
			text: "Visit anonymous group",
			events: { "click": roomSelected }
		}];
		updateElement(page_content, { content });
		return;
	}

	content = rooms.map((x) => ({
		tag: "div",
		class: "chat-room",
		"data-id": x.roomId,
		content: getRoomUI(x),
		events: { "click": roomSelected }
	}));

	updateElement(page_content, { content });
}

async function fetchRooms() {
	const response = await _fetch("/api/rooms");

	if (!response.ok) {
		showProblemDetail(response);
		return []; // pretend an empty list
	}

	const data = await response.json();
	store.putRooms(data.rooms);
	await setRooms(data.rooms);
}

async function openHomePage() {
	const page = openPage('home', { level: 1 });
	if (page.childElementCount) {
		return;
	}
	page.addEventListener("page-back", fetchRooms);
	page.classList.add("flex-column");

	const content = [
		{
			tag: "div", class: "page-header",
			content: [
				{ tag: "span", html: "DRIIMA" },
				{
					tag: "select", class: "app-language",
					props: { value: currentLanguage },
					events: { "change": (e) => changeLanguage(e.target.value)},
					content: [
						{ value: "en", html: "EN" },
						{ value: "fr", html: "FR" }
					]
				}
			]
		},
		{
			tag: "div",
			class: "page-content",
			style: "padding: 0",
			callback: (elem) => page_content = elem
		}
	];
	updateElement(page, { content });

	await store.getRooms().then(setRooms);
}

export default openHomePage;

