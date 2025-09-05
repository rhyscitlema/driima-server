
ALTER TABLE `Rooms` ADD COLUMN LatestMessageId BINARY(16) NULL;
ALTER TABLE `Rooms` ADD FOREIGN KEY (LatestMessageId) REFERENCES Messages(Id) ON DELETE SET NULL;

CALL drop_fk_for_column('Groups', 'WallpaperImageId');
ALTER TABLE `Groups` DROP COLUMN WallpaperImageId;

ALTER TABLE `Rooms` ADD COLUMN WallpaperImageId BIGINT NULL;
ALTER TABLE `Rooms` ADD FOREIGN KEY (WallpaperImageId) REFERENCES Files(Id) ON DELETE SET NULL;

UPDATE Rooms SET Name = '' WHERE Name IS NULL;
ALTER TABLE Rooms MODIFY COLUMN Name VARCHAR(63) NOT NULL;
ALTER TABLE Rooms ADD CONSTRAINT UQ_GroupId_Name UNIQUE (GroupId, Name);

CREATE VIEW ViewRooms AS
SELECT
	r.Id,
	r.GroupId,
	r.Name as RoomName,
	g.Name as GroupName,
	g.About as GroupAbout,
	g.Status as GroupStatus,
	g.JoinKey,
	r.State as RoomState,
	m.DateSent as LatestDateSent,
	IF(m.DateDeleted IS NULL, LEFT(m.Content, 128), NULL) AS LatestMessage,
	HEX(r.SkippedMessageId) as SkippedMessageId,
	logo.Path as GroupLogo,
	banner.Path as GroupBanner
FROM Rooms as r
JOIN `Groups` as g on r.GroupId = g.Id
LEFT JOIN Messages as m on m.Id = r.LatestMessageId
LEFT JOIN FilePaths as logo on logo.Id = g.LogoImageId
LEFT JOIN FilePaths as banner on banner.Id = g.BannerImageId;

CREATE VIEW ViewRoomMembers AS
SELECT
	r.Id as RoomId,
	gm.MemberId,
	gm.Status as MemberStatus,
	rm.DateMuted,
	rm.DatePinned
FROM Rooms as r
JOIN GroupMembers as gm on gm.GroupId = r.GroupId
LEFT JOIN RoomMembers as rm on rm.RoomId = r.Id and rm.MemberId = gm.MemberId;

