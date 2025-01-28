import sys
import psycopg2 	# base de datos postgreSQL
import numpy as np 	# para matrices
import pandas as pd 	# estructuras de datos como DataFrames
import time 		# para agregar pausas entre iteraciones del proceso
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))) 
from resolver_trilateracion import resolver_trilateracion 	
from contextlib import contextmanager 		

class PositionCalculator:	
		
    def __init__(self, db_config):		# __init__ es el constructor de la clase
        self.db_config = db_config		# db_config : 
        				# configuración de la base de datos (nombre, usuario, contraseña) para conectarse a postgreSQL
        
    @contextmanager
    def get_db_connection(self):	# para que las conexiones a la base de datos siempre se cierren, incluso si ocurre un error
        conn = psycopg2.connect(**self.db_config)
        try:
            yield conn
        finally:
            conn.close()
           
            
    
    def get_table_data(self, cursor):	
        """se obtienen los datos de las tablas devices y data_tag"""
        cursor.execute('SELECT id, mac, id_type, positionx, positiony FROM devices') # para realizar consultas SQL de la tabla devices
        
        devices = pd.DataFrame(cursor.fetchall(), 
			columns=['id', 'mac', 'id_type', 'positionx', 'positiony']) # se obtienen los registros de la consulta
        
        cursor.execute('SELECT id, id_src, id_dst, distance_cm, rtt_ns FROM data_tag') # realizar consultas SQL de la tabla data_tag
        
        data_tag = pd.DataFrame(cursor.fetchall(), 
			columns=['id', 'id_src', 'id_dst', 'distance_cm', 'rtt_ns']) # se obtienen los registros de la consulta
        
        data_tag = data_tag.dropna(subset=['id', 'id_src', 'id_dst', 'distance_cm']) # se eliminan los valores nulos de data_tag
        
        return devices, data_tag
        
        
    
    def calculate_distances(self, data_tag, tag_ids, anchor_ids):
        """Se calcula la matriz de distancias promedio entre tags y anchors de manera optimizada"""
    
        filtered_data = data_tag[
        # se seleccionan las filas del DataFrame donde los ids id_src están en tag_ids ; y los ids id_dst están en anchor_ids
        data_tag['id_src'].isin(tag_ids) & data_tag['id_dst'].isin(anchor_ids)
        ]
    
        grouped = filtered_data.groupby(['id_src', 'id_dst'])['distance_cm'].mean().reset_index()
        # grouped es un DataFrame que contiene tan solo los promedios de distancia para cada combinación única de tag y anchor
        pivot = grouped.pivot(index='id_src', columns='id_dst', values='distance_cm')  # distancias promedio
        pivot = pivot.reindex(index=tag_ids, columns=anchor_ids)
        
        distances_cm = pivot.values  # se calcula el promedio de las distancias
        distances_m= distances_cm / 100.0
    
        return distances_m

    
    def update_tag_positions(self, cursor, tag_ids, positions):
        """se actualizan las posiciones de los tags en la base de datos"""
        for tag_id, (x, y) in zip(tag_ids, positions):
            if not (np.isnan(x) or np.isnan(y)):		# si no es nan la posición calculada
                x_rounded = float(round(float(x), 2))		# se redondea a dos decimales
                y_rounded = float(round(float(y), 2))		# se redondea a dos decimales
                tag_id_int = int(tag_id)			# conversión de ID a entero
                
                # consulta SQL para actualizar las posiciones positionx y positiony del dispositivo en la tabla devices
                
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
                        
                        devices_df, data_tag_df = self.get_table_data(cursor) 	# método para ejecutar consultas SQL y recuperar dos tablas
                        
                        anchors = devices_df[devices_df['id_type'] == 1]	# se dividen las tablas de los anchors
                        tags = devices_df[devices_df['id_type'] == 2]		# se dividen las tablas de los tags
                        
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

