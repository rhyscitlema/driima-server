import { _fetch, showProblemDetail } from 'fetch';

class DataStore {
	#rooms = null;
	#messages = {};

	constructor() {
	}

	async fetchRooms(setRooms) {
		const response = await _fetch("/api/rooms");

		if (!response.ok) {
			showProblemDetail(response);
			return []; // pretend an empty list
		}

		const data = await response.json();
		this.#rooms = data.rooms;
		setRooms(this.#rooms);
	}

	getRooms() {
		return Promise.resolve(this.#rooms);
	}

	getMessages(roomId) {
		return Promise.resolve(this.#messages[roomId]);
	}

	putMessages(content) {
		const roomId = content.roomInfo.id;
		const data = this.#messages[roomId];
		if (data == undefined)
			this.#messages[roomId] = content;
		else {
			content.messages.forEach(message => {
				data.messages.push(message);
			});
		}
	}
}

const store = new DataStore();
export default store;
