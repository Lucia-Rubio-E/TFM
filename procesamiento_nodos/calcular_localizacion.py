import sys
import psycopg2
import numpy as np
import pandas as pd
import time
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))) 
from resolver_trilateracion import resolver_trilateracion
from contextlib import contextmanager

class PositionCalculator:
    
    def __init__(self, db_config):
        self.db_config = db_config
        
    @contextmanager
    def get_db_connection(self):
        conn = psycopg2.connect(**self.db_config)
        try:
            yield conn
        finally:
            conn.close()
               
    def get_table_data(self, cursor):
        """se obtienen los datos de las tablas devices y data_tag"""
        cursor.execute('SELECT id, mac, id_type, positionx, positiony FROM devices')
        
        devices = pd.DataFrame(cursor.fetchall(), 
			columns=['id', 'mac', 'id_type', 'positionx', 'positiony'])
        
        cursor.execute('SELECT id, id_src, id_dst, distance_cm, rtt_ns FROM data_tag')
        
        data_tag = pd.DataFrame(cursor.fetchall(), 
			columns=['id', 'id_src', 'id_dst', 'distance_cm', 'rtt_ns'])
        
        data_tag = data_tag.dropna(subset=['id', 'id_src', 'id_dst', 'distance_cm'])
        
        return devices, data_tag
        
    def calculate_distances(self, data_tag, tag_ids, anchor_ids):
        """Se calcula la matriz de distancias promedio entre tags y anchors de manera optimizada"""
    
        filtered_data = data_tag[
        data_tag['id_src'].isin(tag_ids) & data_tag['id_dst'].isin(anchor_ids)
        ]
    
        grouped = filtered_data.groupby(['id_src', 'id_dst'])['distance_cm'].mean().reset_index()
        pivot = grouped.pivot(index='id_src', columns='id_dst', values='distance_cm')
        pivot = pivot.reindex(index=tag_ids, columns=anchor_ids)
        
        distances_cm = pivot.values
        distances_m = distances_cm / 100.0
    
        return distances_m
    
    def update_tag_positions(self, cursor, tag_ids, positions):
        """se actualizan las posiciones de los tags en la base de datos"""
        for tag_id, (x, y) in zip(tag_ids, positions):
            if not (np.isnan(x) or np.isnan(y)):
                x_rounded = float(round(float(x), 2))
                y_rounded = float(round(float(y), 2))
                tag_id_int = int(tag_id)
                
                cursor.execute("""				
                    UPDATE devices 
                    SET positionx = %s, positiony = %s 
                    WHERE id = %s AND id_type = 2
                """, (x_rounded, y_rounded, tag_id_int))
    
    def run(self):
        """cálculo de posiciones"""
        while True:
            try:
                with self.get_db_connection() as conn:
                    with conn.cursor() as cursor:
                        
                        devices_df, data_tag_df = self.get_table_data(cursor)
                        
                        anchors = devices_df[devices_df['id_type'] == 1]
                        tags = devices_df[devices_df['id_type'] == 2]
                        
                        if anchors.empty or tags.empty:
                            print("No hay suficientes dispositivos para calcular posiciones")
                            time.sleep(3)
                            continue
                        
                        tag_ids = tags['id'].astype(int).values
                        anchor_ids = anchors['id'].astype(int).values
                        
                        distances = self.calculate_distances(
                            data_tag_df, 
                            tag_ids,
                            anchor_ids
                        )
                        
                        positions = []
                        anchor_positions = anchors[['positionx', 'positiony']].values.astype(float)
                        
                        for distances_to_anchors in distances:
                            x, y = resolver_trilateracion(
                                anchor_positions[:, 0],
                                anchor_positions[:, 1],
                                distances_to_anchors
                            )
                            positions.append([x, y])
                        
                        self.update_tag_positions(cursor, tag_ids, positions)
                        conn.commit()
                        
                        print("Posiciones actualizadas correctamente")
                        print("Posiciones calculadas:", positions)
                        
            except Exception as e:
                print(f"Error en el proceso: {e}")
                
            time.sleep(3)



if __name__ == "__main__":
    db_config = {
        'dbname': 'postgres2',
        'user': 'postgres',
        'password': 'lucia',
        'host': '127.0.0.1',
        'port': 5432
    }
    
    calculator = PositionCalculator(db_config)
    calculator.run()

