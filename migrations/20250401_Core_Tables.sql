
ALTER DATABASE CHARACTER SET utf8mb4;

CREATE TABLE Files (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Folder VARCHAR(63) NULL,
	Name VARCHAR(127) NOT NULL UNIQUE,
	DisplayName VARCHAR(255) NULL,
	ContentType VARCHAR(127) NULL,
	UploaderId BIGINT NULL,
	DateUploaded TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateDeleted TIMESTAMP NULL,
	Size BIGINT NULL,
	Pages INT NULL, -- for a document
	Width INT NULL, -- for an image
	Height INT NULL,
	Preview BLOB NULL
);

CREATE TABLE Locations (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Type INT NOT NULL, -- Continent, Country, Region, City, Street
	Name VARCHAR(127) NOT NULL,
	ParentId BIGINT NULL,
	Latitude DECIMAL(10, 8) NULL,
	Longitude DECIMAL(11, 8) NULL,
	DateStored TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	UNIQUE KEY UQ_Locations_ParentId_Name (ParentId, Name),
	FOREIGN KEY (ParentId) REFERENCES Locations(Id) ON DELETE CASCADE
);

CREATE TABLE Devices (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Type INT NOT NULL, -- Unknown, Server, Mobile, Desktop, Browser
	DateCreated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL, -- set only when a version is updated
	AppName VARCHAR(63) NULL, -- Firefox, Chrome, Safari, Edge, Brave
	AppVersion VARCHAR(15) NULL,
	PlatformName VARCHAR(15) NULL, -- Android, iOS, Windows, Linux, MacOS
	PlatformVersion VARCHAR(15) NULL,
	DeviceName VARCHAR(63) NULL,
	DeviceModel VARCHAR(15) NULL,
	DeviceVersion VARCHAR(15) NULL,
	DeviceManufacturer VARCHAR(63) NULL,
	DeviceIdentifier VARCHAR(63) NULL UNIQUE
);

CREATE TABLE Users (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	Type INT NOT NULL DEFAULT 1, -- Unknown, Normal, Anonymous, Server
	Status INT NOT NULL DEFAULT 1, -- Unknown, Active, Deleted, Blocked
	AccountId BINARY(16) NULL UNIQUE,
	Name VARCHAR(63) NULL UNIQUE,
	About TEXT NULL,
	DateCreated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL,
	LastConnected TIMESTAMP NULL,
	ProfileImageId BIGINT NULL,
	FOREIGN KEY (ProfileImageId) REFERENCES Files(Id) ON DELETE SET NULL
);

CREATE TABLE Sessions (
	Id BIGINT PRIMARY KEY AUTO_INCREMENT,
	UserId BIGINT NOT NULL,

	DeviceId BIGINT NULL,
	LocationId BIGINT NULL,

	Status INT NOT NULL DEFAULT 1, -- Unknown, Active, LoggedOut, Blocked
	DateCreated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
	DateUpdated TIMESTAMP NULL,

	IPAddress VARCHAR(39) NULL,
	Language VARCHAR(15) NULL,
	Timezone VARCHAR(63) NULL,
	TimezoneOffset INT NULL,

	PushNotificationToken VARCHAR(1023) NULL,
	DateTokenProvided TIMESTAMP NULL,

	FOREIGN KEY (UserId) REFERENCES Users(Id) ON DELETE CASCADE,
	FOREIGN KEY (DeviceId) REFERENCES Devices(Id) ON DELETE CASCADE,
	FOREIGN KEY (LocationId) REFERENCES Locations(Id) ON DELETE SET NULL
);

ALTER TABLE Files ADD FOREIGN KEY (UploaderId) REFERENCES Sessions(Id) ON DELETE SET NULL;

CREATE VIEW FilePaths AS
SELECT Id,
	IF (Folder IS NULL, Name, CONCAT(Folder, '/', Name)) AS `Path`
FROM Files;

