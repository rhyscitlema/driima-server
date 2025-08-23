
CREATE PROCEDURE drop_fk_for_column(
	IN in_table_name VARCHAR(64),
	IN in_column_name VARCHAR(64)
)
BEGIN
	DECLARE fk_name VARCHAR(64);

	-- Look up the foreign key constraint name
	SELECT CONSTRAINT_NAME
		INTO fk_name
		FROM information_schema.KEY_COLUMN_USAGE
	WHERE TABLE_SCHEMA = DATABASE()
		AND TABLE_NAME = in_table_name
		AND COLUMN_NAME = in_column_name
		AND REFERENCED_TABLE_NAME IS NOT NULL
	LIMIT 1;

	-- If found, drop it dynamically
	IF fk_name IS NOT NULL THEN
		SET @sql = CONCAT(
			'ALTER TABLE `', in_table_name,
			'` DROP FOREIGN KEY ', fk_name);
		PREPARE stmt FROM @sql;
		EXECUTE stmt;
		DEALLOCATE PREPARE stmt;
	END IF;
END

