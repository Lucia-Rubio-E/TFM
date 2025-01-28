import numpy as np

def resolver_trilateracion(nodos_x, nodos_y, distancias):
    x1, x2, x3 = nodos_x[:3]
    y1, y2, y3 = nodos_y[:3]
    d1, d2, d3 = distancias[:3]

    A1 = [-2 * (x1 - x2), -2 * (y1 - y2)]
    b1 = d1**2 - d2**2 - x1**2 + x2**2 - y1**2 + y2**2

    A2 = [-2 * (x1 - x3), -2 * (y1 - y3)]
    b2 = d1**2 - d3**2 - x1**2 + x3**2 - y1**2 + y3**2

    A = np.array([A1, A2])
    b = np.array([b1, b2])

    try:
        sol = np.linalg.solve(A, b)
        return sol[0], sol[1]
    except np.linalg.LinAlgError:
        return np.nan, np.nan

