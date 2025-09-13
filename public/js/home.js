import openChatPage from 'chat';
import { openPage } from 'pages';
import { currentLanguage, changeLanguage } from 'i18n';
import { toast, createElement, updateElement } from 'spart';
import { _fetch, sendData, showProblemDetail } from 'fetch';

function roomSelected(e) {
	const params = new URLSearchParams();
	params.set("r", e.target.dataset.id);
	openChatPage(params);
};

let page_content = null;

async function fetchRooms() {
	const response = await _fetch("/api/rooms");

	if (!response.ok) {
		showProblemDetail(response);
		return;
	}

	const data = await response.json();

	const content = data.rooms.map((x) => ({
		tag: "div",
		class: "chat-room",
		"data-id": x.roomId,
		content: [{ text: x.groupName }],
		events: { "click": roomSelected }
	}));

	updateElement(page_content, { content });
}

export default function openHomePage() {
	const page = openPage('home', { level: 1 });
	if (page.childElementCount) {
		fetchRooms();
		return;
	}
	page.addEventListener("page-back", fetchRooms);
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
			tag: "div", class: "page-content",
			content: [{ text: "Loading..." }],
			callback: (elem) => {
				page_content = elem;
				fetchRooms();
			}
		}
	];
	updateElement(page, { content });
}

