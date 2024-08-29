from setuptools import setup, find_packages

setup(
    name='fincept-investments',  
    version='0.1.0',  
    author='Fincept Corporation',  
    author_email='dev@fincept.in',  
    description='A CLI for financial insights and market analysis.',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    url='https://github.com/yourusername/fincept-investments',  # Update with your GitHub repo URL
    packages=find_packages(),
    classifiers=[
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.6',
    install_requires=[
        # Your dependencies
        'requests',
        'rich',
        'gnews',
        'click',
    ],
    entry_points={
        'console_scripts': [
            'fincept=fincept.cli:start',  # This maps the 'fincept' command to the start function in cli.py
        ],
    },
    include_package_data=True,
    package_data={
        '': ['*.txt', '*.md'],
    },
)
