-- Vaciar la tabla data_tag y reiniciar su secuencia
DELETE FROM data_tag;
SELECT setval('public.data_tag_id_seq', 1, false); -- Asegúrate de que el nombre de la secuencia sea correcto

-- Vaciar la tabla devices y reiniciar su secuencia
DELETE FROM devices;
SELECT setval('public.devices_id_seq', 1, false); -- Asegúrate de que el nombre de la secuencia sea correcto

