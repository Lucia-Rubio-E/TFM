DELETE FROM data_tag;
SELECT setval('public.data_tag_id_seq', 1, false); 

DELETE FROM devices;
SELECT setval('public.devices_id_seq', 1, false); 

