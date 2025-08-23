
ALTER TABLE `Rooms` ADD COLUMN LatestMessageId BINARY(16) NULL;
ALTER TABLE `Rooms` ADD FOREIGN KEY (LatestMessageId) REFERENCES Messages(Id) ON DELETE SET NULL;

CALL drop_fk_for_column('Groups', 'WallpaperImageId');
ALTER TABLE `Groups` DROP COLUMN WallpaperImageId;

ALTER TABLE `Rooms` ADD COLUMN WallpaperImageId BIGINT NULL;
ALTER TABLE `Rooms` ADD FOREIGN KEY (WallpaperImageId) REFERENCES Files(Id) ON DELETE SET NULL;

CREATE VIEW ViewRoms AS
SELECT
	gm.MemberId,
	r.Id as RoomId,
	g.Id as GroupId,
	r.Name as RoomName,
	g.Name as GroupName,
	g.Status as GroupStatus,
	gm.Status as GroupMemberStatus,
	rm.DateMuted,
	rm.DatePinned,
	m.DateSent as LatestDateSent,
	logo.Path as GroupLogo,
	banner.Path as GroupBanner
FROM Rooms as r
JOIN `Groups` as g on r.GroupId = g.Id
LEFT JOIN GroupMembers as gm on gm.GroupId = g.Id
LEFT JOIN RoomMembers as rm on r.GroupId = g.Id and rm.MemberId = gm.MemberId
LEFT JOIN Messages as m on m.Id = r.LatestMessageId
LEFT JOIN FilePaths as logo on logo.Id = g.LogoImageId
LEFT JOIN FilePaths as banner on banner.Id = g.BannerImageId;

