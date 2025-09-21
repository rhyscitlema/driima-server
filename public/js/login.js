import openPage from 'pages';
import openHomePage from 'home';
import openChatPage from 'chat';
import { setup, toast, createElement, updateElement } from 'spart';
import { isAuthenticated, sendParams, showProblemDetail } from 'fetch';

function onKeyPressed(e) {
	if (e.key === 'Enter')
		handleLogin();
}

const login_username = 'login-username';
const login_password = 'login-password';
const new_account_txt = 'Create an anonymous account';

export function openLoginPage() {
	const page = openPage('login', { level: 1 });

	if (page.childElementCount)
		return;

	const loginContainer = createElement({
		tag: 'div', id: 'login-container',
		events: { 'keypress': onKeyPressed },
		content: [{
			tag: 'div', id: 'login-form',
			content: [
				{ tag: 'h2', text: 'Login' },
				{
					tag: 'div', class: 'form-group',
					content: [
						{ tag: 'label', for: login_username, content: [{ tag: 'span', text: 'Username' }, { text: ':' }] },
						{ tag: 'input', id: login_username, placeholder: "Username", props: { autofocus: "true" } }
					]
				},
				{
					tag: 'div', class: 'form-group',
					content: [
						{ tag: 'label', for: login_password, content: [{ tag: 'span', text: 'Password' }, { text: ':' }] },
						{ tag: 'input', id: login_password, placeholder: "Password", type: 'password' }
					]
				},
				{
					tag: 'div', class: 'form-buttons',
					content: [
						{
							tag: 'button', id: 'login-btn', text: 'Login',
							events: { 'click': handleLogin }
						},
						{
							tag: 'button', id: 'create-anonymous-btn', text: new_account_txt,
							events: { 'click': createNewAnonymousAccount }
						}
					]
				}
			]
		}]
	});
	page.appendChild(loginContainer);
}

function createNewAnonymousAccount(e) {
	const username = document.getElementById(login_username);
	const password = document.getElementById(login_password);
	const elem = e.target;
	const cancel = "Cancel";

	if (elem.dataset.i18nText == cancel) {
		username.value = '';
		password.value = '';
		username.readOnly = false;
		password.readOnly = false;
		password.type = 'password'; // hide password
		updateElement(elem, { text: new_account_txt });
	}
	else {
		username.value = 'ANO';
		password.value = crypto.randomUUID().replace(/-/g, '');
		username.readOnly = true;
		password.readOnly = true;
		password.type = 'text'; // show password
		updateElement(elem, { text: cancel });
	}
}

async function handleLogin() {
	const username = document.getElementById(login_username).value.trim();
	const password = document.getElementById(login_password).value.trim();

	if (!username || !password) {
		toast("Username and password are required");
		return;
	}

	const params = new URLSearchParams([
		['username', username],
		['password', password]
	]);

	const response = await sendParams("/api/account/login", "POST", params);
	if (response.ok) {
		await onAuthenticated();
	}
	else showProblemDetail(response);
}

function onAuthenticated() {
	const homePage = openHomePage();
	const params = new URLSearchParams(window.location.search);
	const openChat = params.get("g") || params.get("r");
	return openChat ? openChatPage(params) : homePage;
}

// Initialize the chat application
export async function initializeApp(config) {
	await setup(config);

	if (isAuthenticated()) {
		await onAuthenticated();
	}
	else openLoginPage();
}
