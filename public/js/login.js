import { initializeElements, fetchMessages } from 'chat';
import { setup, toast, createElement, updateElement } from 'spart';
import { sendParams, showProblemDetail } from 'fetch';

function generateGUID() {
	return ([1e7] + -1e3 + -4e3 + -8e3 + -1e11).replace(/[018]/g, c =>
		(c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
	);
}

function onKeyPressed(e) {
	if (e.key === 'Enter')
		handleLogin();
}

const login_username = 'login-username';
const login_password = 'login-password';
const new_account_txt = 'Create an anonymous account';

export function createLoginUI() {
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
						{ tag: 'label', for: login_username, content: [{ text: 'Username' }, { text: ':' }] },
						{ tag: 'input', id: login_username, placeholder: "Username" }
					]
				},
				{
					tag: 'div', class: 'form-group',
					content: [
						{ tag: 'label', for: login_password, content: [{ text: 'Password' }, { text: ':' }] },
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
							tag: 'button', id: 'create-anonymous-btn',
							text: new_account_txt,
							events: { 'click': createNewAnonymousAccount }
						}
					]
				}
			]
		}]
	});
	document.body.appendChild(loginContainer);
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
		password.value = generateGUID().replace(/-/g, '');
		username.readOnly = true;
		password.readOnly = true;
		password.type = 'text'; // show password
		updateElement(elem, { text: cancel });
	}
}

function handleLogin() {
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

	sendParams("/api/account/login", "POST", params)
		.then(response => {
			if (response.ok) {
				window.isAuthenticated = true;
				document.getElementById('login-container').remove();

				fetchMessages();
				setInterval(fetchMessages, 4000);
			}
			else showProblemDetail(response);
		});
}

// Initialize the chat application
export function initializeApp() {
	setup().then(() => {
		initializeElements();

		if (window.isAuthenticated) {
			// Start normal chat flow
			fetchMessages();
			setInterval(fetchMessages, 4000);
		}
		else {
			createLoginUI();
		}
	});

	// Make the body height match the visual viewport height
	window.visualViewport.addEventListener('resize', () => {
		document.body.style.height = `${window.visualViewport.height}px`;
	});
}
