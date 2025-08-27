import openChatPage from 'chat';
import { openPage } from 'pages';
import { currentLanguage, changeLanguage } from 'i18n';
import { toast, createElement, updateElement } from 'spart';
import { _fetch, sendData, showProblemDetail } from 'fetch';

function roomSelected(e) {
	const r = e.target.dataset;
	openChatPage({ roomId: r.id, roomName: r.name });
};

async function fetchRooms(container) {
	const response = await _fetch("/api/room/all");

	if (!response.ok) {
		showProblemDetail(response);
		return;
	}

	const data = await response.json();

	const content = data.rooms.map((x) => ({
		tag: "div",
		class: "chat-room",
		"data-id": x.roomId,
		"data-name": x.groupName,
		content: [{ text: x.groupName }],
		events: { "click": roomSelected }
	}));

	updateElement(container, { content });
}

export default function openHomePage() {
	const page = openPage('login', { level: 1 });
	if (page.childElementCount)
		return;
	page.classList.add("flex-column");

	const content = [
		{
			tag: "div", class: "page-header",
			content: [
				{ tag: "span", text: "DRIIMA" },
				{
					tag: "select", class: "app-language",
					props: { value: currentLanguage },
					events: { "change": (e) => changeLanguage(e.target.value)},
					content: [
						{ value: "en", text: "EN" },
						{ value: "fr", text: "FR" }
					]
				}
			]
		},
		{
			tag: "div", class: "page-content", text: "Loading...",
			callback: (elem) => fetchRooms(elem)
		}
	];
	updateElement(page, { content });
}

