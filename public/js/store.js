
class DataStore {
	#rooms = null;
	#messages = {};

	constructor() {
	}

	getRooms() {
		return Promise.resolve(this.#rooms);
	}

	putRooms(rooms) {
		this.#rooms = rooms;
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
