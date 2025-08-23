
CREATE TABLE `Groups` (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Type INT NOT NULL, -- Unknown, Normal, Anonymous
	Name VARCHAR(127) NOT NULL,
	JoinKey BIGINT NOT NULL,
	CreatorId BIGINT NULL, -- nullable as a user may ask to be deleted
	DateCreated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL,
	Status INT NOT NULL DEFAULT 1, -- Unknown, Active, Deleted, Blocked
	LogoImageId BIGINT NULL,
	BannerImageId BIGINT NULL,
	WallpaperImageId BIGINT NULL,
	About TEXT NULL,
	FOREIGN KEY (CreatorId) REFERENCES Sessions(Id) ON DELETE SET NULL,
	FOREIGN KEY (LogoImageId) REFERENCES Files(Id) ON DELETE SET NULL,
	FOREIGN KEY (BannerImageId) REFERENCES Files(Id) ON DELETE SET NULL,
	FOREIGN KEY (WallpaperImageId) REFERENCES Files(Id) ON DELETE SET NULL
);

CREATE TABLE Rooms (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	GroupId BIGINT NULL,
	Name VARCHAR(127) NULL,
	State INT NOT NULL DEFAULT 1, -- Unknown, Normal, AIBusy
	DateCreated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL,
	SkippedMessageId BINARY(16) NULL, -- latest message that AI should skip
	FOREIGN KEY (GroupId) REFERENCES `Groups`(Id) ON DELETE CASCADE
);

CREATE TABLE GroupMembers (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	GroupId BIGINT NOT NULL,
	MemberId BIGINT NOT NULL,
	DateAdded TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL,
	Status INT NOT NULL DEFAULT 1, -- Unknown, Active, Exited, Blocked
	UNIQUE KEY UQ_GroupMembers_GroupId_MemberId (GroupId, MemberId),
	FOREIGN KEY (GroupId) REFERENCES `Groups`(Id) ON DELETE CASCADE,
	FOREIGN KEY (MemberId) REFERENCES Users(Id) ON DELETE CASCADE
);

CREATE TABLE RoomMembers (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	RoomId BIGINT NOT NULL,
	MemberId BIGINT NOT NULL,
	DateAdded TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateMuted TIMESTAMP NULL,
	DatePinned TIMESTAMP NULL,
	UNIQUE KEY UQ_RoomMembers_RoomId_MemberId (RoomId, MemberId),
	FOREIGN KEY (RoomId) REFERENCES Rooms(Id) ON DELETE CASCADE,
	FOREIGN KEY (MemberId) REFERENCES Users(Id) ON DELETE CASCADE
);

CREATE TABLE URLs (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Value VARCHAR(255) NOT NULL UNIQUE,
	Title VARCHAR(255) NOT NULL,
	Authors VARCHAR(255),
	Description VARCHAR(1023),
	DateStored TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	Preview BLOB NULL
);

CREATE TABLE Messages (
	Id BINARY(16) NOT NULL PRIMARY KEY,
	RoomId BIGINT NOT NULL,
	SenderId BIGINT NOT NULL,
	AuthorId BIGINT NULL,
	ParentId BINARY(16) NULL,
	Type INT NOT NULL DEFAULT 1, -- Unknown, Normal, ToolCall, Event
	Status INT NOT NULL, -- Unknown, Sent
	DateSent TIMESTAMP(6) NOT NULL,
	DateStored TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6),
	DateDeleted TIMESTAMP(6) NULL,
	DateStarred TIMESTAMP NULL,
	FileId BIGINT NULL,
	UrlId BIGINT NULL,
	Content TEXT NOT NULL,
	INDEX IX_Messages_RoomId_DateSent (RoomId, DateSent),
	FOREIGN KEY (RoomId) REFERENCES Rooms(Id) ON DELETE CASCADE,
	FOREIGN KEY (SenderId) REFERENCES Sessions(Id) ON DELETE CASCADE,
	FOREIGN KEY (AuthorId) REFERENCES Sessions(Id) ON DELETE SET NULL,
	FOREIGN KEY (ParentId) REFERENCES Messages(Id) ON DELETE SET NULL,
	FOREIGN KEY (FileId) REFERENCES Files(Id) ON DELETE SET NULL,
	FOREIGN KEY (UrlId) REFERENCES URLs(Id) ON DELETE SET NULL
);

ALTER TABLE Rooms ADD FOREIGN KEY (SkippedMessageId) REFERENCES Messages(Id) ON DELETE SET NULL;

CREATE TABLE HttpRequests (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	MessageId BINARY(16) NULL,
	DateStored TIMESTAMP(6) DEFAULT CURRENT_TIMESTAMP(6),
	URL VARCHAR(1023) NOT NULL,
	Duration INT NOT NULL,
	StatusCode INT NOT NULL,
	RequestHeaders TEXT NULL,
	ResponseHeaders TEXT NULL,
	RequestContent LONGTEXT NULL,
	ResponseContent LONGTEXT NULL,
	FOREIGN KEY (MessageId) REFERENCES Messages(Id) ON DELETE SET NULL
);

CREATE VIEW ViewMessages AS
SELECT HEX(m.Id) as Id,
	HEX(m.ParentId) as ParentId,
	m.RoomId,
	s.UserId,
	u.Name as UserName,
	m.DateSent,
	m.Status,
	IF(m.DateDeleted IS NULL, m.Content, NULL) AS Content
FROM Messages as m
JOIN Sessions as s on s.Id = m.SenderId
JOIN Users as u on u.Id = s.UserId
WHERE m.Type != 2; -- skip ToolCall

-- --------

INSERT INTO `Users` (Type, Name) VALUES (3, 'AI');
SET @userId = LAST_INSERT_ID();

INSERT INTO `Sessions` (UserId) VALUES (@userId);
SET @sessionId = LAST_INSERT_ID();

INSERT INTO Files (Name, UploaderId) VALUES ('/image/anonymous-chat.jpg', @sessionId);
SET @fileId = LAST_INSERT_ID();

INSERT INTO `Groups` (Type, Name, JoinKey, CreatorId, BannerImageId, About) VALUES (2, 'Anonymous', 0, @sessionId, @fileId,
'Chat freely and anonymously about anything! An AI is also a member of this group. Just start with @ai whenever you''d like assistance, answers, or a second opinion.');
SET @groupId = LAST_INSERT_ID();

INSERT INTO `Rooms` (GroupId) VALUES (@groupId);

