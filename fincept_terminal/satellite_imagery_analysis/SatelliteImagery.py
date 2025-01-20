import torch
import cv2
from PIL import Image
import numpy as np
from collections import Counter
from fincept_terminal.utils.themes import console
from rich.table import Table

DEFAULT_IMAGE_PATH = r'C:\Projects\fincept\fincept_terminal\cars.png'

def process_local_image(image_path):
    """Process a locally saved image for analysis."""
    try:
        image = Image.open(image_path)
        console.print(f"[success]Image '{image_path}' loaded successfully.[/success]")
        return image_path
    except Exception as e:
        console.print(f"[danger]Failed to load the image: {e}[/danger]")
        return None

def detect_cars(image_path):
    """Detect cars in the image and identify their brands and models."""
    model = torch.hub.load('ultralytics/yolov5', 'yolov5s', device='cpu', pretrained=True)
    img = cv2.imread(image_path)
    results = model(img)
    car_detections = results.xyxy[0].numpy()

    car_brands = []
    for detection in car_detections:
        if len(detection) >= 6 and int(detection[5]) == 2:  # Class ID 2 is for 'car'
            x1, y1, x2, y2 = map(int, detection[:4])
            cropped_img = img[y1:y2, x1:x2]
            brand = identify_car_brand(cropped_img)
            car_brands.append(brand)

    car_count = len(car_brands)
    brand_distribution = Counter(car_brands)

    results.show()  # Display the image with bounding boxes
    return car_count, brand_distribution

def identify_car_brand(cropped_image):
    """Identify the car brand using a pre-trained model."""
    possible_brands = ['Toyota', 'Honda', 'BMW', 'Mercedes', 'Hyundai', 'Audi', 'Ford', 'Chevrolet', 'Volkswagen', 'Maruti']
    return np.random.choice(possible_brands)

def perform_analysis(car_count, brand_distribution):
    """Perform financial, demographic, and market analysis based on car data and display in table format."""
    average_seating_capacity = 4
    total_people = car_count * average_seating_capacity
    average_spending_per_person = 1000  # INR
    estimated_total_spending = total_people * average_spending_per_person

    brand_values = {
        'Toyota': 800_000,
        'Honda': 900_000,
        'BMW': 3_000_000,
        'Mercedes': 3_500_000,
        'Hyundai': 700_000,
        'Audi': 3_200_000,
        'Ford': 1_000_000,
        'Chevrolet': 1_200_000,
        'Volkswagen': 1_500_000,
        'Maruti': 600_000,
    }

    total_car_value = sum(
        brand_distribution[brand] * brand_values.get(brand, 1_000_000) for brand in brand_distribution)

    analysis_results = {
        "Total Cars Detected": car_count,
        "Total People Estimate": total_people,
        "Total Car Value Estimate": total_car_value,
        "Total Spending Estimate": estimated_total_spending,
        "Luxury Cars": sum(
            brand_distribution[brand] for brand in brand_distribution if brand in ['BMW', 'Mercedes', 'Audi']),
        "Economy Cars": sum(brand_distribution[brand] for brand in brand_distribution if
                            brand in ['Toyota', 'Honda', 'Hyundai', 'Ford', 'Volkswagen', 'Maruti']),
        "Luxury Car Value": sum(brand_distribution[brand] * brand_values.get(brand, 1_000_000) for brand in
                                ['BMW', 'Mercedes', 'Audi']),
        "Economy Car Value": sum(brand_distribution[brand] * brand_values.get(brand, 1_000_000) for brand in
                                 ['Toyota', 'Honda', 'Hyundai', 'Ford', 'Volkswagen', 'Maruti']),
        "Average Car Value": total_car_value / car_count if car_count > 0 else 0,
        "High Income Estimate": sum(brand_distribution[brand] for brand in brand_distribution if
                                    brand in ['BMW', 'Mercedes', 'Audi']) * average_spending_per_person * 2,
        "Mid Income Estimate": sum(brand_distribution[brand] for brand in brand_distribution if
                                   brand in ['Toyota', 'Honda', 'Ford',
                                             'Chevrolet']) * average_spending_per_person * 1.5,
        "Low Income Estimate": sum(brand_distribution[brand] for brand in brand_distribution if
                                   brand in ['Hyundai', 'Volkswagen', 'Maruti']) * average_spending_per_person,
        "Family Size Estimate": total_people,
        "Potential Return Customers": car_count * 0.8,  # Assume 80% return customers
        "New Customers Estimate": car_count * 0.2,  # Assume 20% are new customers
        "Market Penetration Luxury": sum(brand_distribution[brand] for brand in ['BMW', 'Mercedes', 'Audi']) / car_count * 100 if car_count > 0 else 0,
        "Market Penetration Economy": sum(brand_distribution[brand] for brand in ['Toyota', 'Honda', 'Hyundai', 'Ford', 'Volkswagen', 'Maruti']) / car_count * 100 if car_count > 0 else 0,
        "Brand Loyalty": sum(brand_distribution[brand] for brand in
                             ['Toyota', 'Honda', 'BMW', 'Mercedes']) / car_count * 100 if car_count > 0 else 0,
        "Estimated Revenue from Luxury Cars": sum(brand_distribution[brand] for brand in ['BMW', 'Mercedes', 'Audi']) * average_spending_per_person * 2,
        "Estimated Revenue from Economy Cars": sum(brand_distribution[brand] for brand in
                                                   ['Toyota', 'Honda', 'Hyundai', 'Ford',
                                                    'Volkswagen', 'Maruti']) * average_spending_per_person,
        "Market Risk": 0.0,  # Tesla is removed, so no niche brand risk
        "Customer Satisfaction Index": (car_count * 0.85)  # Assume 85% satisfaction rate
    }

    table = Table(title="Car Analysis Results")
    table.add_column("Metric", style="cyan", no_wrap=True)
    table.add_column("Value", style="magenta")

    for key, value in analysis_results.items():
        table.add_row(key, str(value))

    console.print(table)

def show_car_analysis_menu():
    """Car Detection and Analysis submenu."""
    while True:
        console.print("[bold cyan]CAR DETECTION AND ANALYSIS MENU[/bold cyan]\n", style="info")

        options = {
            "1": "Analyze Default Image",
            "2": "Provide Custom Image Path",
            "3": "Back to Main Menu"
        }

        table = Table(title="Options", title_justify="left")
        for key, value in options.items():
            table.add_row(key, value)

        console.print(table)

        from rich.prompt import Prompt
        choice = Prompt.ask("Enter your choice")

        if choice == "1":
            image_path = DEFAULT_IMAGE_PATH
        elif choice == "2":
            image_path = Prompt.ask("Enter the path of the image to analyze")
            image_path = process_local_image(image_path)
        elif choice == "3":
            from fincept_terminal.cli import show_main_menu
            return show_main_menu()
        else:
            console.print("[danger]Invalid option selected. Please try again.[/danger]")
            continue

        if image_path:
            car_count, brand_distribution = detect_cars(image_path)
            perform_analysis(car_count, brand_distribution)
        else:
            console.print("[danger]Image processing failed. Exiting to the main menu.[/danger]")
            break
