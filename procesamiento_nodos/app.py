from flask import Flask, request, jsonify
import psycopg2

app = Flask(__name__)

db_params = {  
    'dbname': 'postgres2',
    'user': 'postgres',
    'password': 'lucia',
    'host': '127.0.0.1',
    'port': 5432
}

@app.get('/device_position')
def get_device_position():

    # parámetros de consulta : mac , id o num_device   
    mac = request.args.get('mac')
    device_id = request.args.get('id')
    num_device = request.args.get('num_device')

    if not mac and not device_id and not num_device: # si no se consulta por ningún parámetro 
        return jsonify({'error': 'No se ha consultado por mac, id o num_device como parámetro'}), 400

    try:
        
        # conexión a la basde de datos PostgreSQL
        conn = psycopg2.connect(**db_params)
        cursor = conn.cursor()

        # consulta SQL según el parámetro de entrada
        if mac:
            query = "SELECT positionx, positiony FROM devices WHERE mac = %s;"
            cursor.execute(query, (mac,))
        elif device_id:
            query = "SELECT positionx, positiony FROM devices WHERE id = %s;"
            cursor.execute(query, (device_id,))
        elif num_device:
            query = "SELECT positionx, positiony FROM devices OFFSET %s LIMIT 1;"
            cursor.execute(query, (int(num_device) - 1,))  # índice de fila desde 1

        result = cursor.fetchone() # resultado de la consulta
        if result:
            positionx, positiony = result
            return jsonify({'positionx': positionx, 'positiony': positiony}), 200
        else:
            return jsonify({'error': 'No se ha encontrado el dispositivo consultado'}), 404

    except psycopg2.Error as e:
        print(f"Error de la base de datos: {e}")
        return jsonify({'error': 'Error de la conexión a la base de datos'}), 500

    finally:
        # se cierra la conexión de la base de datos
        if cursor:
            cursor.close()
        if conn:
            conn.close()

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000, debug=True)
